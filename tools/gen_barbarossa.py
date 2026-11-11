#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_barbarossa.py —— 生成《巴巴罗萨行动》三张战区地图的 JSON 数据

思路：
  1. 每张图给定一个经纬度窗口与六角格数，按等距圆柱投影栅格化；
  2. 地形由"地理要素 → 多边形 / 折线"决定（海、湖、沼泽、森林、山地）；
  3. 河流与国界按折线采样，落进哪两个相邻格，就在公共边上打点；
  4. 城市按经纬度吸附到格心，名字中英双列。

输出：maps/巴巴罗萨-北方.json / -中央.json / -南方.json
数据可手改；渲染由 CounterForge.exe --map 完成。
"""
import json
import math
import os

# ── 版面常数（与 C++ 端保持一致）────────────────────────────────────
COLS, ROWS = 36, 18
MARGIN, TITLE, LEGEND = 12.0, 34.0, 22.0
R = (594.0 - 2 * MARGIN) / (2 + 1.5 * (COLS - 1))   # 外接圆半径（mm）
HEX_MM = R * math.sqrt(3.0)                          # 对边距
COL_PITCH = 1.5 * R

KM_PER_DEG_LAT = 111.0


def coslat(lat):
    return math.cos(math.radians(lat))


# ── 六角格几何（与 maprenderer.cpp 一致）────────────────────────────
def center_mm(c, r):
    x = MARGIN + R + c * COL_PITCH
    y = MARGIN + TITLE + HEX_MM * 0.5 + r * HEX_MM + (HEX_MM * 0.5 if c & 1 else 0.0)
    return x, y


def inside_hex(px, py, c, r):
    x, y = center_mm(c, r)
    dx, dy = px - x, py - y
    return abs(dy) <= HEX_MM * 0.5 and (abs(dx) + abs(dy) * R / HEX_MM) <= R


def neighbor(c, r, e):
    odd = c & 1
    if e == 0:
        return c, r - 1
    if e == 1:
        return (c + 1, r) if odd else (c + 1, r - 1)
    if e == 2:
        return (c + 1, r + 1) if odd else (c + 1, r)
    if e == 3:
        return c, r + 1
    if e == 4:
        return (c - 1, r + 1) if odd else (c - 1, r)
    return (c - 1, r) if odd else (c - 1, r - 1)


class Grid:
    """经纬度 ↔ 格子 的双向映射"""

    def __init__(self, lon0, lon1, lat0, lat1):
        self.lon0, self.lon1 = lon0, lon1
        self.lat0, self.lat1 = lat0, lat1          # lat0 = 南边界
        self.w_mm = 2 * MARGIN + 2 * R + (COLS - 1) * COL_PITCH
        self.h_mm = 2 * MARGIN + TITLE + LEGEND + ROWS * HEX_MM + HEX_MM * 0.5

    def mm_of(self, lon, lat):
        u = (lon - self.lon0) / (self.lon1 - self.lon0)
        v = (self.lat1 - lat) / (self.lat1 - self.lat0)
        x = MARGIN + R + u * (COLS - 1) * COL_PITCH
        y = MARGIN + TITLE + HEX_MM * 0.5 + v * ROWS * HEX_MM
        return x, y

    def hex_of(self, lon, lat):
        x, y = self.mm_of(lon, lat)
        c0 = int(math.floor((x - MARGIN - R) / COL_PITCH)) - 2
        r0 = int(math.floor((y - MARGIN - TITLE - HEX_MM * 0.5) / HEX_MM)) - 2
        for c in range(c0, c0 + 5):
            if not (0 <= c < COLS):
                continue
            off = HEX_MM * 0.5 if c & 1 else 0.0
            rr0 = int(math.floor((y - MARGIN - TITLE - HEX_MM * 0.5 - off) / HEX_MM)) - 2
            for r in range(rr0, rr0 + 5):
                if 0 <= r < ROWS and inside_hex(x, y, c, r):
                    return c, r
        # 兜底：最近格心（避免采样点落在格缝上被判成地图外，河流在那里断掉）
        best, bd = None, 1e9
        for c in range(max(0, c0), min(COLS, c0 + 6)):
            for r in range(max(0, r0), min(ROWS, r0 + 6)):
                cx, cy = center_mm(c, r)
                d = (cx - x) ** 2 + (cy - y) ** 2
                if d < bd:
                    bd, best = d, (c, r)
        return best

    def latlon_of(self, c, r):
        x, y = center_mm(c, r)
        u = (x - MARGIN - R) / ((COLS - 1) * COL_PITCH)
        v = (y - MARGIN - TITLE - HEX_MM * 0.5) / (ROWS * HEX_MM)
        lon = self.lon0 + u * (self.lon1 - self.lon0)
        lat = self.lat1 - v * (self.lat1 - self.lat0)
        return lon, lat

    def km_per_hex(self):
        mid = (self.lat0 + self.lat1) / 2
        return (self.lat1 - self.lat0) * KM_PER_DEG_LAT / ROWS


# ── 多边形工具 ──────────────────────────────────────────────────────
def in_poly(lon, lat, poly):
    inside = False
    n = len(poly)
    j = n - 1
    for i in range(n):
        xi, yi = poly[i]
        xj, yj = poly[j]
        if ((yi > lat) != (yj > lat)) and \
           (lon < (xj - xi) * (lat - yi) / (yj - yi + 1e-12) + xi):
            inside = not inside
        j = i
    return inside


def any_poly(lon, lat, polys):
    return any(in_poly(lon, lat, p) for p in polys)


def densify(line, step_km=2.0):
    out = []
    for (lon0, lat0), (lon1, lat1) in zip(line, line[1:]):
        dx = (lon1 - lon0) * KM_PER_DEG_LAT * coslat((lat0 + lat1) / 2)
        dy = (lat1 - lat0) * KM_PER_DEG_LAT
        dist = math.hypot(dx, dy)
        n = max(1, int(dist / step_km))
        for i in range(n):
            t = i / n
            out.append((lon0 + (lon1 - lon0) * t, lat0 + (lat1 - lat0) * t))
    out.append(line[-1])
    return out


def link(grid, a, b, store, edge_of):
    """把两个相邻格之间的公共边双向标上"""
    if a is None or b is None or a == b:
        return
    e = edge_of(a, b)
    if e is None:
        return
    store.setdefault(a, set()).add(e)
    back = {0: 3, 1: 4, 2: 5, 3: 0, 4: 1, 5: 2}[e]
    store.setdefault(b, set()).add(back)


def mark_line(grid, line, store, edge_of):
    """沿折线走，跨格时在公共边打点；跨多了就插值补齐，保证线不断"""
    pts = densify(line, 0.35)          # 步长压到 0.35km，保证相邻采样点绝不跳格
    prev = None
    prev_pt = None
    for lon, lat in pts:
        cell = grid.hex_of(lon, lat)
        if cell is None:
            continue
        if prev is not None and cell != prev and edge_of(prev, cell) is None:
            # 跳格：在两点之间插值，逐格连上（密插值 + 双向兜底）
            n = 24
            chain = [prev]
            for i in range(1, n + 1):
                t = i / n
                ilon = prev_pt[0] + (lon - prev_pt[0]) * t
                ilat = prev_pt[1] + (lat - prev_pt[1]) * t
                cc = grid.hex_of(ilon, ilat)
                if cc and cc != chain[-1]:
                    chain.append(cc)
            for a, b in zip(chain, chain[1:]):
                link(grid, a, b, store, edge_of)
        elif prev is not None:
            link(grid, prev, cell, store, edge_of)
        prev, prev_pt = cell, (lon, lat)


def edge_of(a, b):
    for e in range(6):
        if neighbor(*a, e) == b:
            return e
    return None


# ══════════════════════════════════════════════════════════════════
#  地理要素（近似，够用即可；要更准就替换这些坐标）
# ══════════════════════════════════════════════════════════════════

BALTIC = [
    [(19.0, 62.0), (21.0, 61.6), (22.5, 61.0), (23.5, 60.3), (24.5, 59.8),
     (26.0, 59.5), (28.0, 59.4), (30.0, 59.4), (32.0, 59.8), (32.0, 62.0),
     (19.0, 62.0)],
]
# 芬兰湾：狭长海湾，西接波罗的海、东到列宁格勒（30.3E,59.9N）
FINLAND_GULF = [
    [(22.5, 60.3), (24.0, 60.2), (26.0, 60.1), (28.0, 60.0), (29.5, 59.95),
     (30.3, 59.9), (30.3, 59.7), (29.0, 59.7), (27.0, 59.75), (25.0, 59.8),
     (23.5, 59.9), (22.5, 60.0)],
]
RIGA_GULF = [[(22.6, 57.9), (24.4, 57.9), (24.3, 57.1), (23.0, 56.9)]]

BLACK_SEA = [
    [(28.0, 44.0), (28.2, 45.6), (29.7, 45.4), (30.8, 46.2), (31.8, 46.4),
     (33.2, 45.9), (33.6, 44.9), (35.2, 44.6), (36.6, 45.2), (38.0, 46.2),
     (40.5, 46.4), (40.5, 44.0)],
]
AZOV = [[(35.0, 46.6), (37.2, 46.4), (38.6, 47.2), (37.4, 45.7), (35.4, 45.4)]]

LAKES = [
    [(27.2, 58.2), (28.3, 58.4), (28.2, 59.0), (27.3, 58.9)],          # 楚德湖/佩普西湖
    [(30.9, 58.0), (31.6, 58.1), (31.7, 58.5), (31.0, 58.5)],          # 伊尔门湖
    [(32.4, 57.0), (33.4, 57.1), (33.5, 57.4), (32.5, 57.4)],          # 谢利格尔湖
    [(29.8, 60.0), (31.5, 60.1), (32.5, 60.6), (32.6, 61.5), (31.6, 61.6),
     (30.6, 61.6), (30.0, 61.2), (29.5, 60.6)],                        # 拉多加湖（放大修正）
]

PRIPYAT_MARSH = [
    [(24.4, 51.9), (26.0, 52.2), (27.6, 52.1), (29.2, 51.8), (30.4, 51.6),
     (30.6, 51.0), (29.0, 50.9), (27.0, 51.1), (25.4, 51.2), (24.3, 51.4)],
]

FORESTS = [
    [(23.4, 52.4), (24.2, 52.9), (24.0, 53.3), (23.2, 53.1)],          # 比亚沃维耶扎
    [(33.6, 52.4), (34.6, 53.0), (34.4, 53.6), (33.3, 53.2)],          # 布良斯克森林
    [(25.0, 54.0), (27.0, 55.0), (26.0, 55.6), (24.6, 54.6)],          # 纳拉奇湖林区
    [(30.0, 55.4), (32.0, 56.2), (31.4, 56.9), (29.8, 56.0)],          # 斯摩棱斯克北林区
    [(34.0, 56.4), (36.0, 57.0), (35.6, 57.6), (33.8, 57.0)],          # 伏尔加上游森林
]

CARPATHIANS = [
    [(22.4, 49.4), (23.6, 49.0), (25.0, 48.4), (26.2, 48.0), (26.6, 47.6),
     (25.4, 47.4), (24.0, 47.8), (22.8, 48.4), (22.2, 48.9)],
]
HILLS = [
    [(33.6, 56.8), (35.2, 57.2), (35.4, 57.8), (33.8, 57.5)],          # 瓦尔代丘陵
    [(35.0, 51.4), (37.2, 51.8), (37.6, 52.6), (35.4, 52.4)],          # 中俄罗斯高地
    [(37.0, 48.0), (39.0, 48.4), (39.4, 49.2), (37.2, 49.0)],          # 顿涅茨岭
]

SEAS = [BALTIC[0], FINLAND_GULF[0], RIGA_GULF[0]]
BLACK = [BLACK_SEA[0], AZOV[0]]

RIVERS = {
    "vistula": (1, [(49.6, 18.9), (50.3, 19.6), (51.1, 21.0), (52.2, 21.1),
                    (53.0, 19.2), (54.3, 18.7)]),
    "neman":   (2, [(53.9, 25.4), (54.4, 24.6), (55.0, 22.8), (55.4, 21.4)]),
    "dvina":   (1, [(56.6, 32.2), (55.9, 31.0), (55.2, 30.2), (54.8, 29.0),
                    (55.1, 27.6), (55.8, 26.6), (56.4, 25.2), (57.0, 24.2)]),
    "dnieper": (1, [(54.9, 33.2), (54.4, 32.3), (54.0, 31.5), (53.4, 30.8),
                    (52.6, 30.7), (51.6, 30.7), (50.6, 30.6), (49.6, 31.4),
                    (48.6, 33.4), (47.9, 35.2), (46.9, 33.6), (46.6, 32.4)]),
    "pripyat": (2, [(51.9, 24.4), (51.9, 26.4), (52.0, 28.4), (51.6, 29.8),
                    (51.4, 30.6)]),
    "berezina": (2, [(54.6, 28.2), (54.0, 28.7), (53.3, 29.3), (52.9, 30.1)]),
    "sozh":    (2, [(53.9, 31.6), (52.9, 31.1), (52.1, 30.9)]),
    "desna":   (2, [(54.0, 33.6), (53.0, 33.5), (52.0, 32.6), (51.4, 31.4)]),
    "bug":     (2, [(50.4, 24.0), (51.4, 23.8), (52.1, 23.8), (52.5, 21.6)]),
    "san":     (2, [(49.4, 22.6), (50.4, 22.1), (50.7, 21.9)]),
    "don":     (1, [(54.0, 38.6), (52.6, 39.1), (51.0, 39.6), (49.6, 40.6),
                    (48.2, 40.6), (47.3, 39.4)]),
    "donets":  (2, [(50.4, 36.6), (49.6, 37.6), (48.6, 38.2), (47.9, 39.1)]),
    "dniester": (2, [(49.0, 24.4), (48.2, 27.0), (47.2, 28.6), (46.3, 30.0)]),
    "sbug":    (2, [(49.4, 27.4), (48.6, 29.4), (47.6, 31.0), (46.8, 32.0)]),
    "volkhov": (2, [(58.6, 31.3), (58.9, 31.8), (59.4, 32.2)]),
    "luga":    (2, [(58.7, 29.8), (59.2, 29.9), (59.6, 29.4)]),
    "neva":    (2, [(59.9, 30.6), (59.9, 31.0), (59.9, 31.5)]),
}

FRONT_1941 = [(20.6, 55.3), (22.0, 55.1), (22.8, 54.4), (22.9, 53.4),
              (23.2, 52.6), (23.7, 52.1), (24.0, 51.6), (24.4, 50.7),
              (25.2, 49.9), (26.4, 49.3), (27.2, 48.4), (28.1, 47.6),
              (28.4, 46.6), (28.2, 45.6)]

CITIES = [
    # name, alt, lon, lat, size, port, fortress
    ("柏林", "BERLIN", 13.40, 52.52, 5, False, False),
    ("但泽", "DANZIG", 18.65, 54.35, 3, True, False),
    ("柯尼斯堡", "KÖNIGSBERG", 20.51, 54.71, 4, True, True),
    ("华沙", "WARSCHAU", 21.01, 52.23, 5, False, False),
    ("卢布林", "LUBLIN", 22.57, 51.25, 3, False, False),
    ("克拉科夫", "KRAKAU", 19.94, 50.06, 4, False, True),
    ("利沃夫", "LEMBERG", 24.03, 49.84, 3, False, False),
    ("布列斯特", "BREST-LITOWSK", 23.70, 52.10, 3, False, True),
    ("比亚韦斯托克", "BIALYSTOK", 23.16, 53.13, 2, False, False),
    ("维尔纽斯", "WILNA", 25.28, 54.69, 3, False, False),
    ("考纳斯", "KAUEN", 23.90, 54.90, 2, False, False),
    ("里加", "RIGA", 24.11, 56.95, 4, True, False),
    ("塔林", "REVAL", 24.75, 59.44, 3, True, False),
    ("纳尔瓦", "NARWA", 28.19, 59.38, 1, False, False),
    ("普斯科夫", "PSKOW", 28.33, 57.82, 3, False, True),
    ("诺夫哥罗德", "NOWGOROD", 31.27, 58.52, 3, False, False),
    ("卢加", "LUGA", 29.85, 58.74, 1, False, False),
    ("大卢基", "WELIKIJE LUKI", 30.52, 56.34, 2, False, False),
    ("维捷布斯克", "WITEBSK", 30.20, 55.19, 3, False, False),
    ("明斯克", "MINSK", 27.57, 53.90, 4, False, True),
    ("博布鲁伊斯克", "BOBRUISK", 29.22, 53.15, 2, False, True),
    ("莫吉廖夫", "MOGILEW", 30.33, 53.90, 3, False, False),
    ("奥尔沙", "ORSCHA", 30.42, 54.51, 2, False, False),
    ("斯摩棱斯克", "SMOLENSK", 32.05, 54.78, 4, False, True),
    ("维亚济马", "WIASMA", 34.29, 55.21, 2, False, False),
    ("勒热夫", "RSHEW", 34.33, 56.26, 2, False, False),
    ("布良斯克", "BRJANSK", 34.36, 53.24, 3, False, False),
    ("奥廖尔", "OREL", 36.07, 52.97, 2, False, False),
    ("戈梅利", "GOMEL", 30.98, 52.44, 3, False, False),
    ("切尔尼戈夫", "TSCHERNIGOW", 31.29, 51.49, 2, False, False),
    ("基辅", "KIEW", 30.52, 50.45, 5, True, True),
    ("日托米尔", "SHITOMIR", 28.66, 50.25, 2, False, False),
    ("文尼察", "WINNIZA", 28.47, 49.23, 2, False, False),
    ("罗夫诺", "ROWNО", 26.25, 50.62, 2, False, False),
    ("塔尔诺波尔", "TARNOPOL", 25.60, 49.55, 2, False, False),
    ("乌曼", "UMAN", 30.22, 48.75, 1, False, False),
    ("第聂伯罗彼得罗夫斯克", "DNJEPROPETROWSK", 35.05, 48.46, 4, False, False),
    ("扎波罗热", "SAPOROSCHJE", 35.14, 47.84, 3, False, False),
    ("克里沃罗格", "KRIWOI ROG", 33.39, 47.91, 2, False, False),
    ("尼古拉耶夫", "NIKOLAJEW", 32.00, 46.97, 2, True, False),
    ("敖德萨", "ODESSA", 30.73, 46.48, 4, True, True),
    ("塞瓦斯托波尔", "SEWASTOPOL", 33.53, 44.62, 3, True, True),
    ("辛菲罗波尔", "SIMFEROPOL", 34.10, 44.95, 2, False, False),
    ("刻赤", "KERTSCH", 36.47, 45.36, 2, True, False),
    ("哈尔科夫", "CHARKOW", 36.23, 49.99, 4, False, False),
    ("波尔塔瓦", "POLTAWA", 34.55, 49.59, 2, False, False),
    ("基希讷乌", "KISCHINJOW", 28.86, 47.01, 2, False, False),
    ("莫斯科", "MOSKAU", 37.62, 55.75, 5, False, True),
    ("加里宁", "KALININ", 35.90, 56.86, 3, False, False),
    ("列宁格勒", "LENINGRAD", 30.32, 59.94, 5, True, True),
]

THEATERS = [
    {
        "file": "巴巴罗萨-北方.json",
        "title": "巴巴罗萨行动 · 北方集团军群",
        "subtitle": "Unternehmen Barbarossa — Heeresgruppe Nord",
        "date": "1941 年 6 月 22 日 · 东普鲁士 — 列宁格勒",
        "note": "格边 18 mm · 1 格 = 30 km",
        "window": (19.6, 35.0, 54.6, 62.0),
        "labels": [
            ("波罗的海", 22.5, 58.5, 7.5, -12),
            ("东 普 鲁 士", 20.6, 54.9, 5.0, 0),
            ("立 陶 宛", 24.2, 55.5, 5.0, 0),
            ("拉 脱 维 亚", 25.2, 57.4, 5.0, 0),
            ("爱 沙 尼 亚", 26.0, 59.0, 5.0, 0),
            ("普斯科夫州", 29.4, 57.2, 4.6, 0),
            ("芬 兰 湾", 27.0, 60.0, 5.0, 0),
            ("拉 多 加 湖", 31.0, 60.8, 4.6, 0),
            ("卡累利阿", 31.5, 61.3, 4.6, 0),
        ],
    },
    {
        "file": "巴巴罗萨-中央.json",
        "title": "巴巴罗萨行动 · 中央集团军群",
        "subtitle": "Unternehmen Barbarossa — Heeresgruppe Mitte",
        "date": "1941 年 6 月 22 日 · 布格河 — 斯摩棱斯克",
        "note": "格边 18 mm · 1 格 = 30 km",
        "window": (21.4, 36.2, 50.6, 56.0),
        "labels": [
            ("波 兰", 21.9, 51.6, 5.4, 0),
            ("白 俄 罗 斯", 27.4, 54.6, 6.0, 0),
            ("普里皮亚季沼泽", 26.6, 51.5, 4.8, -8),
            ("斯摩棱斯克高地", 33.2, 55.1, 4.6, 0),
            ("布 良 斯 克 林 区", 34.2, 53.6, 4.4, 0),
            ("第 聂 伯 河", 30.4, 52.2, 4.0, 72),
        ],
    },
    {
        "file": "巴巴罗萨-南方.json",
        "title": "巴巴罗萨行动 · 南方集团军群",
        "subtitle": "Unternehmen Barbarossa — Heeresgruppe Süd",
        "date": "1941 年 6 月 22 日 · 加利西亚 — 基辅 — 黑海",
        "note": "格边 18 mm · 1 格 = 30 km",
        "window": (24.0, 39.4, 45.0, 50.8),
        "labels": [
            ("加 利 西 亚", 24.9, 49.4, 5.0, 0),
            ("乌 克 兰", 31.0, 49.4, 6.4, 0),
            ("喀尔巴阡山", 24.2, 48.2, 4.4, -18),
            ("黑 海", 33.2, 45.6, 6.4, 0),
            ("亚 速 海", 37.0, 46.4, 4.0, 0),
            ("克里木", 34.2, 45.3, 4.6, 0),
            ("第 聂 伯 河", 32.2, 47.6, 4.0, -60),
        ],
    },
]


def build(theater):
    lon0, lon1, lat0, lat1 = theater["window"]
    grid = Grid(lon0, lon1, lat0, lat1)
    km = grid.km_per_hex()

    terrain = [["." for _ in range(COLS)] for _ in range(ROWS)]
    rivers, borders = {}, {}

    for r in range(ROWS):
        for c in range(COLS):
            lon, lat = grid.latlon_of(c, r)
            t = "."
            if any_poly(lon, lat, SEAS) or any_poly(lon, lat, BLACK):
                t = "~"
            elif any_poly(lon, lat, LAKES):
                t = "~"
            elif any_poly(lon, lat, PRIPYAT_MARSH):
                t = "m"
            elif any_poly(lon, lat, CARPATHIANS):
                t = "^"
            elif any_poly(lon, lat, HILLS):
                t = "h"
            elif any_poly(lon, lat, FORESTS):
                t = "f"
            else:
                # 用低频噪声做成片分布：北部森林带、南部耕地/草原、中部混合
                h = (math.sin(c * 12.9898 + r * 78.233) * 43758.5453) % 1.0
                n = (math.sin(c * 0.62 + r * 0.81) + math.sin(c * 1.7 - r * 1.3) * 0.5) * 0.5 + 0.5
                if lat > 55.0:
                    t = "f" if (n > 0.34 or h < 0.45) else "."
                elif lat > 53.4:
                    t = "f" if (n > 0.48 or h < 0.38) else ("h" if n < 0.20 else ".")
                elif lat > 52.0:
                    t = "f" if n > 0.60 else ("m" if n < 0.16 and h < 0.4 else ".")
                elif lat > 50.9:
                    t = "," if n > 0.40 else ("f" if n > 0.78 else ".")
                else:
                    t = "," if n > 0.22 else "."
                if h < 0.06:
                    t = "m"
            terrain[r][c] = t

    major_store = {}
    for name, (cls, line) in RIVERS.items():
        # RIVERS 里为了好写，坐标是 (lat, lon)，这里统一换成 (lon, lat)
        line = [(lon, lat) for (lat, lon) in line]
        store = {}
        mark_line(grid, line, store, edge_of)
        for cell, edges in store.items():
            rivers.setdefault(cell, set()).update(edges)
            if cls == 1:
                major_store.setdefault(cell, set()).update(edges)

    mark_line(grid, FRONT_1941, borders, edge_of)

    data = {
        "format": "counterforge.map/1",
        "title": theater["title"],
        "subtitle": theater["subtitle"],
        "date": theater["date"],
        "note": theater["note"],
        "cols": COLS,
        "rows": ROWS,
        "hexMm": round(HEX_MM, 2),
        "marginMm": MARGIN,
        "titleMm": TITLE,
        "legendMm": LEGEND,
        "kmPerHex": round(km, 1),
        "paper": "A2",
        "terrain": ["".join(row) for row in terrain],
        "rivers": [[c, r, e] for (c, r), es in sorted(rivers.items()) for e in sorted(es)],
        "riverMajor": [[c, r, e] for (c, r), es in sorted(major_store.items()) for e in sorted(es)],
        "borders": [[c, r, e] for (c, r), es in sorted(borders.items()) for e in sorted(es)],
        "rail": [],
        "roads": [],
        "cities": [],
        "labels": [],
    }

    # 铁路（粗线：华沙—明斯克—斯摩棱斯克—莫斯科；布列斯特—戈梅利；基辅—第聂伯）
    rail_lines = [
        [(21.0, 52.2), (23.7, 52.1), (27.6, 53.9), (30.4, 54.5), (32.1, 54.8), (34.3, 55.2), (35.9, 56.9)],
        [(23.7, 52.1), (27.6, 53.9), (29.2, 53.2), (30.9, 52.4), (31.3, 51.5)],
        [(24.0, 50.6), (28.7, 50.3), (30.5, 50.5), (32.0, 49.0), (35.1, 48.5)],
        [(24.1, 56.9), (25.3, 56.0), (28.3, 57.8), (30.5, 56.3), (32.1, 54.8)],
        [(30.5, 50.5), (31.0, 49.6), (34.6, 49.6), (36.2, 50.0)],
    ]
    rail_store = {}
    for line in rail_lines:
        mark_line(grid, line, rail_store, edge_of)
    data["rail"] = [[c, r, e] for (c, r), es in sorted(rail_store.items()) for e in sorted(es)]

    # 公路（几条主干）
    road_lines = [
        [(21.0, 52.2), (23.2, 52.6), (23.7, 52.1), (26.0, 52.6), (27.6, 53.9)],
        [(27.6, 53.9), (30.4, 54.5), (32.1, 54.8)],
        [(30.5, 50.5), (28.7, 50.3), (26.3, 50.6)],
        [(32.1, 54.8), (34.3, 55.2)],
    ]
    road_store = {}
    for line in road_lines:
        mark_line(grid, line, road_store, edge_of)
    data["roads"] = [[c, r, e] for (c, r), es in sorted(road_store.items()) for e in sorted(es)]

    # 城市
    for name, alt, lon, lat, size, port, fort in CITIES:
        if not (lon0 <= lon <= lon1 and lat0 <= lat <= lat1):
            continue
        cell = grid.hex_of(lon, lat)
        if cell is None:
            continue
        city = {"name": name, "alt": alt, "col": cell[0], "row": cell[1], "size": size}
        if port:
            city["port"] = True
        if fort:
            city["fortress"] = True
        data["cities"].append(city)

    # 地区名
    for text, lc, lr, size, angle in theater["labels"]:
        cell = grid.hex_of(lc, lr)
        if cell is None:
            cell = (int((lc - lon0) / (lon1 - lon0) * COLS), int((lat1 - lr) / (lat1 - lat0) * ROWS))
        label = {"text": text, "col": max(0, min(COLS - 1, cell[0])),
                 "row": max(0, min(ROWS - 1, cell[1])), "size": size}
        if angle:
            label["angle"] = angle
        data["labels"].append(label)

    return data


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    outdir = os.path.join(root, "maps")
    os.makedirs(outdir, exist_ok=True)
    for th in THEATERS:
        data = build(th)
        path = os.path.join(outdir, th["file"])
        with open(path, "w", encoding="utf-8") as f:
            json.dump(data, f, ensure_ascii=False, indent=1)
        print(f"{th['file']}: {len(data['cities'])} 城 · {len(data['rivers'])} 河段 · "
              f"{len(data['rail'])} 铁路边 · {data['kmPerHex']} km/格")


if __name__ == "__main__":
    main()
