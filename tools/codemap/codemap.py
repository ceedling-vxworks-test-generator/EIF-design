"""Cのソースを解析して、部品ごとのコード構成をHTML（1ファイル）に出力する。

使い方:
    python tools/codemap/codemap.py tools/codemap/eif.json -o 出力.html

外部ライブラリは使わない（Python標準ライブラリだけ）。解析は簡易な字句解析で、次を取り出す。
  - 関数（定義場所、static か、Doxygen の @brief／@note、実行コンテキスト [C1]〜[C4]）
  - 呼出し（直接呼出し、構造体メンバ経由の呼出し、設定ファイルで補う間接呼出し）
  - 型（typedef struct／enum／関数ポインタ）
  - 表（ファイルスコープの const 配列＝変換表・定義表、static な配列＝実行時データ、関数ポインタの表＝IFの実装）
  - ガード（if の中で EIF_ERR_*／UC_ERR_*／false 等を return する箇所）
  - #include（依存規則の確認）
"""
import argparse
import json
import os
import re
import sys
from collections import defaultdict

KEYWORDS = {"if", "for", "while", "switch", "return", "sizeof", "do", "else", "case", "_Alignas", "alignas",
            "defined", "static_assert", "_Static_assert", "__attribute__", "typeof", "offsetof"}
GUARD_RET = re.compile(r"^(EIF_ERR_\w+|UC_ERR_\w+|false|NULL|EIF_ACCEPTED|EIF_OK|r|result|0U?|-1)$")
ERR_RET = re.compile(r"^(EIF_ERR_\w+|UC_ERR_\w+|false)$")


# ------------------------------------------------------------------ 字句の前処理
def mask_comments_and_strings(src):
    """コメントと文字列の中身を空白に置き換える（位置と改行は保つ）。"""
    out = list(src)
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if src.startswith("//", i):
            j = src.find("\n", i)
            j = n if j < 0 else j
            for k in range(i, j):
                out[k] = " "
            i = j
        elif src.startswith("/*", i):
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            for k in range(i, j):
                if src[k] != "\n":
                    out[k] = " "
            i = j
        elif c in "\"'":
            q = c
            j = i + 1
            while j < n and src[j] != q:
                if src[j] == "\\":
                    j += 1
                j += 1
            for k in range(i + 1, min(j, n)):
                if src[k] != "\n":
                    out[k] = " "
            i = j + 1
        elif c == "#":
            # プリプロセッサ行は、#include を除いて空白にする（マクロの中の括弧で数がずれないように）
            j = src.find("\n", i)
            j = n if j < 0 else j
            while j > 0 and src[j - 1] == "\\" and j < n:
                j2 = src.find("\n", j + 1)
                j = n if j2 < 0 else j2
            if not src.startswith("#include", i):
                for k in range(i, j):
                    if src[k] != "\n":
                        out[k] = " "
            i = j
        else:
            i += 1
    return "".join(out)


def brace_depths(code):
    d = 0
    depth = [0] * (len(code) + 1)
    for i, c in enumerate(code):
        depth[i] = d
        if c == "{":
            d += 1
        elif c == "}":
            d -= 1
    depth[len(code)] = d
    return depth


def match_close(code, i, open_c="{", close_c="}"):
    d = 0
    for j in range(i, len(code)):
        if code[j] == open_c:
            d += 1
        elif code[j] == close_c:
            d -= 1
            if d == 0:
                return j
    return len(code) - 1


def line_of(src, pos):
    return src.count("\n", 0, pos) + 1


def doxygen_before(src, pos):
    """pos の直前にある /** ... */ を返す（空白だけを挟む場合）。"""
    k = pos
    while k > 0 and src[k - 1] in " \t\r\n":
        k -= 1
    if src[max(0, k - 2):k] != "*/":
        # 同じ行の前（static bool f(...) { の上の1行コメント）も見る
        return ""
    s = src.rfind("/**", 0, k)
    if s < 0:
        return ""
    return src[s + 3:k - 2]


def parse_doc(doc):
    if not doc:
        return "", ""
    lines = [re.sub(r"^\s*\*\s?", "", l).rstrip() for l in doc.split("\n")]
    text = "\n".join(lines).strip()
    brief = ""
    m = re.search(r"@brief\s+(.*?)(?=\n\s*\n|\n\s*@|$)", text, re.S)
    if m:
        brief = " ".join(m.group(1).split())
    elif text and not text.startswith("@"):
        brief = text.split("\n\n")[0].replace("\n", " ").strip()
    notes = " ".join(" ".join(x.split()) for x in re.findall(r"@note\s+(.*?)(?=\n\s*\n|\n\s*@|$)", text, re.S))
    return brief, notes


# ------------------------------------------------------------------ 1ファイルの解析
FUNC_RE = re.compile(r"(?P<prefix>(?:^|[;}\n])\s*(?P<head>(?:static\s+|inline\s+|extern\s+)*[A-Za-z_][\w\s\*]*?))"
                     r"\b(?P<name>[A-Za-z_]\w*)\s*\((?P<args>[^;{}()]*(?:\([^()]*\)[^;{}()]*)*)\)\s*\{", re.M)
TYPEDEF_BLOCK_RE = re.compile(r"\btypedef\s+(struct|enum|union)\s*\w*\s*\{")
TYPEDEF_FNPTR_RE = re.compile(r"\btypedef\s+[^;{}]*?\(\s*\*\s*(\w+)\s*\)\s*\([^;]*\)\s*;")
TYPEDEF_SIMPLE_RE = re.compile(r"\btypedef\s+(?!struct|enum|union)[\w\s\*]+?\b(\w+)\s*;")
ARRAY_INIT_RE = re.compile(r"(?:^|[;}\n])\s*(?P<q>(?:static\s+)?(?:const\s+)?)(?P<type>(?:struct\s+)?[A-Za-z_]\w*)(?:\s+|\s*\*\s*)"
                           r"(?P<name>[A-Za-z_]\w*)\s*\[[^\]]*\]\s*=\s*\{", re.M)
STRUCT_INIT_RE = re.compile(r"(?:^|[;}\n])\s*(?P<q>(?:static\s+)?const\s+)(?P<type>[A-Za-z_]\w*)\s+"
                            r"(?P<name>[A-Za-z_]\w*)\s*=\s*\{", re.M)
ANON_ARRAY_RE = re.compile(r"(?:^|[;}\n])\s*(?P<q>static\s+const\s+)struct\s*\{")
STATIC_ARRAY_RE = re.compile(r"(?:^|[;}\n])\s*static\s+(?:_Alignas\([^)]*\)\s+|alignas\([^)]*\)\s+)?(?P<type>[A-Za-z_]\w*)\s+"
                             r"(?P<name>[A-Za-z_]\w*)\s*\[[^\]]+\]\s*;", re.M)
INCLUDE_RE = re.compile(r'#include\s+([<"])([^>"]+)[>"]')
CALL_RE = re.compile(r"(?:(?P<recv>[A-Za-z_]\w*)\s*(?:\(\s*\))?\s*(?P<arrow>->|\.)\s*)?\b(?P<name>[A-Za-z_]\w*)\s*\(")
INIT_TYPED_RE = re.compile(r"(?:const\s+)?(?P<type>[A-Za-z_]\w*)\s+[A-Za-z_]\w*\s*=\s*\{")
DESIG_RE = re.compile(r"\.(\w+)\s*=\s*([A-Za-z_]\w*)\s*[,}\n]")


def parse_file(path, rel):
    src = open(path, encoding="utf-8", errors="replace").read()
    code = mask_comments_and_strings(src)
    # extern "C" { … } は波括弧の深さに数えない（ヘッダーの宣言を拾うため）
    for m in re.finditer(r'extern\s+"\s*"\s*\{', code):
        ob = m.end() - 1
        cb = match_close(code, ob)
        code = code[:ob] + " " + code[ob + 1:cb] + " " + code[cb + 1:]
    depth = brace_depths(code)
    res = {"file": rel, "functions": [], "types": [], "tables": [], "includes": [], "desig": [],
           "brief": parse_doc(re.search(r"/\*\*(.*?)\*/", src, re.S).group(1))[0] if re.search(r"/\*\*(.*?)\*/", src, re.S) else ""}
    for m in INCLUDE_RE.finditer(code):
        res["includes"].append({"name": m.group(2), "system": m.group(1) == "<", "line": line_of(src, m.start())})
    # 関数定義
    for m in FUNC_RE.finditer(code):
        name = m.group("name")
        start = m.start("head")
        if name in KEYWORDS or depth[m.start("name")] != 0:
            continue
        head = m.group("head")
        if re.search(r"\b(typedef|struct|enum|union|return|else)\b", head) or "=" in head:
            continue
        ob = m.end() - 1
        cb = match_close(code, ob)
        doc = doxygen_before(src, start)
        brief, notes = parse_doc(doc)
        ctx = re.findall(r"\[(C\d)\]", doc)
        res["functions"].append({
            "name": name, "static": "static" in head.split(), "line": line_of(src, start),
            "brief": brief, "notes": notes, "ctx": ctx[0] if ctx else "",
            "body_start": ob, "body_end": cb, "_code": code[ob:cb + 1], "_src": src[ob:cb + 1],
            "_offset": ob,
        })
    # 型
    for m in TYPEDEF_BLOCK_RE.finditer(code):
        if depth[m.start()] != 0:
            continue
        ob = m.end() - 1
        cb = match_close(code, ob)
        tm = re.match(r"\s*(\w+)\s*;", code[cb + 1:])
        if not tm:
            continue
        brief, _ = parse_doc(doxygen_before(src, m.start()))
        res["types"].append({"name": tm.group(1), "kind": m.group(1), "line": line_of(src, m.start()), "brief": brief})
    for m in TYPEDEF_FNPTR_RE.finditer(code):
        if depth[m.start()] != 0:
            continue
        brief, _ = parse_doc(doxygen_before(src, m.start()))
        res["types"].append({"name": m.group(1), "kind": "関数ポインタ", "line": line_of(src, m.start()), "brief": brief})
    for m in TYPEDEF_SIMPLE_RE.finditer(code):
        if depth[m.start()] != 0 or "(" in m.group(0):
            continue
        brief, _ = parse_doc(doxygen_before(src, m.start()))
        res["types"].append({"name": m.group(1), "kind": "別名", "line": line_of(src, m.start()), "brief": brief})
    # 表
    seen = set()

    def add_table(name, kind, typ, pos, q):
        if name in seen:
            return
        seen.add(name)
        brief, _ = parse_doc(doxygen_before(src, pos))
        res["tables"].append({"name": name, "kind": kind, "type": typ, "line": line_of(src, pos), "brief": brief,
                              "static": "static" in q})
    for m in ARRAY_INIT_RE.finditer(code):
        if depth[m.start("q")] == 0 and m.group("type") not in KEYWORDS:
            add_table(m.group("name"), "const_array" if "const" in m.group("q") else "array_init",
                      m.group("type"), m.start("q"), m.group("q"))
    for m in STRUCT_INIT_RE.finditer(code):
        if depth[m.start("q")] == 0:
            add_table(m.group("name"), "ops", m.group("type"), m.start("q"), m.group("q"))
    for m in ANON_ARRAY_RE.finditer(code):
        if depth[m.start("q")] != 0:
            continue
        ob = code.find("{", m.end() - 1)
        cb = match_close(code, ob)
        nm = re.match(r"\s*(\w+)\s*\[\]\s*=", code[cb + 1:])
        if nm:
            add_table(nm.group(1), "const_array", "struct", m.start("q"), m.group("q"))
    for m in STATIC_ARRAY_RE.finditer(code):
        if depth[m.start()] == 0 and m.group("type") not in ("unsigned", "char"):
            add_table(m.group("name"), "runtime", m.group("type"), m.start("type"), "static")
    # static な非配列の実行時データ（構造体の表）は対象外。unsigned char の作業領域も対象外
    # 初期化子の中の .member = 関数 を、初期化する型ごとに集める（IFの実装表、EIF_ConnectorDesc 等）
    for m in INIT_TYPED_RE.finditer(code):
        ob = m.end() - 1
        cb = match_close(code, ob)
        for d in DESIG_RE.finditer(code[ob:cb + 1]):
            res["desig"].append((m.group("type"), d.group(1), d.group(2)))
    return res


# ------------------------------------------------------------------ ガード
def find_guards(fn):
    """if (...) の直後で、エラーを表す値を return する箇所を拾う。"""
    code, src = fn["_code"], fn["_src"]
    out = []
    for m in re.finditer(r"\bif\s*\(", code):
        po = m.end() - 1
        pc = match_close(code, po, "(", ")")
        cond = " ".join(src[po + 1:pc].split())
        rest = code[pc + 1:]
        k = len(rest) - len(rest.lstrip())
        if rest[k:k + 1] == "{":
            bc = match_close(rest, k)
            body = rest[k:bc + 1]
            bsrc = src[pc + 1 + k:pc + 1 + bc + 1]
        else:
            semi = rest.find(";")
            body = rest[:semi + 1]
            bsrc = src[pc + 1:pc + 1 + semi + 1]
        rm = re.search(r"\breturn\s+([^;]+);", body)
        if not rm:
            continue
        val = " ".join(rm.group(1).split())
        # 戻り値が関数呼出し（to_uc(r) 等）でも、エラー定数が見えるものを拾う
        consts = re.findall(r"\b(EIF_ERR_\w+|UC_ERR_\w+)\b", val)
        if not (ERR_RET.match(val) or consts or val in ("false",) or re.match(r"^(r|m)$", val)):
            continue
        if val in ("r", "m") and not re.search(r"!=|==", cond):
            continue
        comment = ""
        cm = re.search(r"/\*\s*(.*?)\s*\*/", bsrc)
        if cm:
            comment = cm.group(1)
        out.append({"cond": cond, "ret": val, "comment": comment, "pos": m.start(),
                    "line_off": code.count("\n", 0, m.start())})
    return out


# ------------------------------------------------------------------ 全体の解析
def analyze(cfg, base):
    root = os.path.join(base, cfg["root"])
    files = []
    for d, _, fs in os.walk(root):
        for f in fs:
            if not f.endswith((".c", ".h")):
                continue
            rel = os.path.relpath(os.path.join(d, f), root).replace("\\", "/")
            if any(rel.startswith(x) for x in cfg.get("exclude", [])):
                continue
            files.append(parse_file(os.path.join(d, f), rel))

    def comp_of(rel):
        for c in cfg["components"]:
            if any(rel.startswith(p) for p in c["prefix"]):
                return c["key"]
        return "other"

    def ext_of(name):
        for c in cfg["externals"]:
            if any(name.startswith(p) for p in c["prefix"]):
                return c["key"]
        return None

    funcs = {}
    for f in files:
        f["comp"] = comp_of(f["file"])
        for fn in f["functions"]:
            fn["file"] = f["file"]
            fn["comp"] = f["comp"]
            key = fn["name"]
            if key in funcs:
                # 同名の static 関数（on_event 等）は「ファイル::名前」で区別する
                key = f"{fn['name']}@{f['file']}"
            fn["id"] = key
            funcs[key] = fn

    def resolve(name, from_file=None):
        """名前から関数IDを引く。同じファイルの static を優先する。"""
        cands = [k for k, v in funcs.items() if v["name"] == name]
        if not cands:
            return []
        same = [k for k in cands if funcs[k]["file"] == from_file]
        return same or cands

    # 構造体メンバ → 実装関数（.submit = ctl_submit 等）
    member_impl = defaultdict(set)   # (型, メンバ) → 実装関数
    by_member = defaultdict(set)     # メンバ → 実装関数（受け手の型が分からないとき用）
    for f in files:
        for typ, mem, rhs in f["desig"]:
            for fid in resolve(rhs, f["file"]):
                member_impl[(typ, mem)].add(fid)
                by_member[mem].add(fid)
    recv_types = cfg.get("receiver_types", {})
    ignore_funcs = set(cfg.get("ignore_functions", []))

    tables = {}
    for f in files:
        for t in f["tables"]:
            t["file"] = f["file"]
            t["comp"] = f["comp"]
            if t["kind"] == "ops":
                t["category"] = "impl"     # 関数ポインタの表＝IFの実装
            elif t["kind"] == "runtime":
                t["category"] = "run"
            elif "/models/" in f["file"] or f["comp"] in ("con", "acc"):
                t["category"] = "conv"
            else:
                t["category"] = "def"
            tables[t["name"]] = t
    conv_pats = [re.compile(p) for p in cfg.get("conversion_function_patterns", [])]
    entry = set()
    for f in files:
        if f["file"] == cfg.get("entry_header"):
            for m in re.finditer(r"\b(EIF_\w+)\s*\(", mask_comments_and_strings(
                    open(os.path.join(root, f["file"]), encoding="utf-8").read())):
                entry.add(m.group(1))

    calls = {}
    for fid, fn in funcs.items():
        seq = []
        for m in CALL_RE.finditer(fn["_code"]):
            nm = m.group("name")
            if nm in KEYWORDS or nm == fn["name"]:
                continue
            pos = m.start("name")
            if m.group("arrow"):
                typ = recv_types.get(m.group("recv") or "")
                impls = member_impl.get((typ, nm), set()) if typ else set()
                if not impls and not typ and len(by_member.get(nm, ())) == 1:
                    impls = by_member[nm]
                for impl in sorted(impls):
                    seq.append({"to": impl, "pos": pos, "via": nm, "kind": "member"})
                continue
            if nm in ignore_funcs:
                continue
            targets = resolve(nm, fn["file"])
            if targets:
                seq.append({"to": targets[0], "pos": pos, "kind": "direct"})
            else:
                ek = ext_of(nm)
                if ek:
                    seq.append({"to": "ext:" + nm, "ext": ek, "pos": pos, "kind": "external"})
        for nm in cfg.get("indirect_calls", {}).get(fn["name"], []):
            for t in resolve(nm):
                if funcs[t]["file"] != fn["file"] or True:
                    seq.append({"to": t, "pos": len(fn["_code"]) - 2, "kind": "indirect"})
        # 同じ呼出し先は最初の1回だけ（同じ関数の中で何度も呼ぶ場合）
        uniq, seen = [], set()
        for c in sorted(seq, key=lambda c: c["pos"]):
            if c["to"] in seen:
                continue
            seen.add(c["to"])
            uniq.append(c)
        calls[fid] = uniq
        # 参照している表
        used = []
        for tname in tables:
            mm = re.search(r"\b" + re.escape(tname) + r"\b", fn["_code"])
            if mm:
                used.append({"table": tname, "pos": mm.start()})
        fn["tables_used"] = used
        fn["guards"] = find_guards(fn)
        fn["is_entry"] = fn["name"] in entry and not fn["static"]
        fn["is_conv"] = any(p.search(fn["name"]) for p in conv_pats)

    callers = defaultdict(list)
    for fid, cs in calls.items():
        for c in cs:
            callers[c["to"]].append(fid)

    # 依存規則
    path_comp = {f["file"]: f["comp"] for f in files}
    basename_to_file = {}
    for f in files:
        basename_to_file.setdefault(os.path.basename(f["file"]), f["file"])
    deps = []
    rules = cfg.get("dependency_rules", {})
    model_only = cfg.get("model_only_external", {})
    for f in files:
        for inc in f["includes"]:
            if inc["system"]:
                continue
            target = basename_to_file.get(os.path.basename(inc["name"]))
            ok, why = True, ""
            r = rules.get(f["comp"])
            if target:
                tcomp = path_comp[target]
                if r and not any(target.startswith(a) for a in r["allow_dirs"]) and not target.startswith(
                        f["file"].rsplit("/", 1)[0] + "/"):
                    ok, why = False, f"{f['comp']} から {target} は参照できない"
                deps.append({"from": f["file"], "fcomp": f["comp"], "to": target, "tcomp": tcomp, "ok": ok,
                             "why": why, "line": inc["line"], "external": False})
            else:
                if r and inc["name"] not in r["allow_external"]:
                    ok, why = False, f"{f['comp']} から外部ヘッダー {inc['name']} は参照できない"
                if inc["name"] in model_only and model_only[inc["name"]] not in f["file"]:
                    ok, why = False, cfg.get("dependency_notes", {}).get(inc["name"], "参照できない場所")
                deps.append({"from": f["file"], "fcomp": f["comp"], "to": inc["name"], "tcomp": "外部", "ok": ok,
                             "why": why, "line": inc["line"], "external": True})

    # 出力用に整える
    out_funcs = {}
    for fid, fn in funcs.items():
        out_funcs[fid] = {
            "id": fid, "name": fn["name"], "file": fn["file"], "line": fn["line"], "comp": fn["comp"],
            "static": fn["static"], "brief": fn["brief"], "notes": fn["notes"], "ctx": fn["ctx"],
            "entry": fn["is_entry"], "conv": fn["is_conv"],
            "calls": [{k: v for k, v in c.items() if k != "pos"} | {"line": fn["line"] + fn["_code"].count("\n", 0, c["pos"])}
                      for c in calls[fid]],
            "callers": sorted(set(callers.get(fid, []))),
            "guards": [{"cond": g["cond"], "ret": g["ret"], "comment": g["comment"], "line": fn["line"] + g["line_off"]}
                       for g in fn["guards"]],
            "tables": sorted({u["table"] for u in fn["tables_used"]}),
            "events": sorted(
                [{"t": "call", "i": i, "pos": c["pos"]} for i, c in enumerate(calls[fid])]
                + [{"t": "guard", "i": i, "pos": g["pos"]} for i, g in enumerate(fn["guards"])]
                + [{"t": "table", "name": u["table"], "pos": u["pos"]} for u in fn["tables_used"]],
                key=lambda e: e["pos"]),
        }
        for e in out_funcs[fid]["events"]:
            e.pop("pos", None)
    types = []
    for f in files:
        for t in f["types"]:
            types.append(t | {"file": f["file"], "comp": f["comp"]})
    return {
        "title": cfg.get("title", "コード構成"),
        "components": cfg["components"], "externals": cfg["externals"], "lane_order": cfg["lane_order"],
        "contexts": cfg.get("contexts", {}), "async_contexts": cfg.get("async_contexts", []),
        "external_callers": cfg.get("external_callers", {}), "flows": cfg.get("flows", []),
        "files": [{"file": f["file"], "comp": f["comp"], "brief": f["brief"]} for f in files],
        "functions": out_funcs, "types": types, "tables": list(tables.values()), "deps": deps,
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("config")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--base", default=None, help="config の root の基準フォルダ（既定：config の2つ上＝リポジトリ）")
    a = ap.parse_args()
    cfg = json.load(open(a.config, encoding="utf-8"))
    base = a.base or os.path.abspath(os.path.join(os.path.dirname(a.config), "..", ".."))
    data = analyze(cfg, base)
    tpl = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "template.html"), encoding="utf-8").read()
    html = tpl.replace("/*__DATA__*/null", json.dumps(data, ensure_ascii=False)).replace("__TITLE__", data["title"])
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    open(a.out, "w", encoding="utf-8").write(html)
    nf = len(data["functions"])
    ng = sum(len(f["guards"]) for f in data["functions"].values())
    nv = sum(1 for d in data["deps"] if not d["ok"])
    print(f"関数 {nf}、型 {len(data['types'])}、表 {len(data['tables'])}、ガード {ng}、依存規則の違反 {nv} → {a.out}")


if __name__ == "__main__":
    sys.exit(main())
