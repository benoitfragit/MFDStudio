#!/usr/bin/env python3
from __future__ import annotations

import argparse
import datetime as dt
import re
import urllib.request
from pathlib import Path

from docx import Document
from docx.enum.section import WD_SECTION
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement, parse_xml
from docx.oxml.ns import nsdecls, qn
from docx.shared import Cm, Inches, Pt, RGBColor


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build the detailed MFDStudio user guide DOCX.")
    parser.add_argument("--source", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--rendered-diagram-dir", required=True)
    parser.add_argument("--skip-diagram-rendering", action="store_true")
    return parser.parse_args()


def resolve_path(repo_root: Path, raw_path: str) -> Path:
    path = Path(raw_path)
    if path.is_absolute():
        return path
    return (repo_root / path).resolve()


def render_diagrams(repo_root: Path, output_dir: Path) -> None:
    diagram_dir = repo_root / "docs" / "user_guide" / "diagrams"
    output_dir.mkdir(parents=True, exist_ok=True)
    for diagram_file in sorted(diagram_dir.glob("*.puml")):
        request = urllib.request.Request(
            url="https://kroki.io/plantuml/png",
            data=diagram_file.read_text(encoding="utf-8").encode("utf-8"),
            headers={"Content-Type": "text/plain"},
            method="POST",
        )
        output_file = output_dir / f"{diagram_file.stem}.png"
        with urllib.request.urlopen(request) as response:
            output_file.write_bytes(response.read())


def set_cell_shading(cell, fill: str) -> None:
    cell._tc.get_or_add_tcPr().append(parse_xml(rf'<w:shd {nsdecls("w")} w:fill="{fill}"/>'))


def set_cell_margins(cell, top: int = 80, start: int = 120, bottom: int = 80, end: int = 120) -> None:
    tc_pr = cell._tc.get_or_add_tcPr()
    tc_mar = tc_pr.first_child_found_in("w:tcMar")
    if tc_mar is None:
        tc_mar = OxmlElement("w:tcMar")
        tc_pr.append(tc_mar)
    for tag, value in (("top", top), ("start", start), ("bottom", bottom), ("end", end)):
        node = tc_mar.find(qn(f"w:{tag}"))
        if node is None:
            node = OxmlElement(f"w:{tag}")
            tc_mar.append(node)
        node.set(qn("w:w"), str(value))
        node.set(qn("w:type"), "dxa")


def strip_inline_markdown(text: str) -> str:
    return text.replace("**", "").replace("`", "")


def add_toc(paragraph) -> None:
    run = paragraph.add_run()
    fld_begin = OxmlElement("w:fldChar")
    fld_begin.set(qn("w:fldCharType"), "begin")

    instr = OxmlElement("w:instrText")
    instr.set(qn("xml:space"), "preserve")
    instr.text = r'TOC \o "1-3" \h \z \u'

    fld_separate = OxmlElement("w:fldChar")
    fld_separate.set(qn("w:fldCharType"), "separate")

    fld_end = OxmlElement("w:fldChar")
    fld_end.set(qn("w:fldCharType"), "end")

    run._r.append(fld_begin)
    run._r.append(instr)
    run._r.append(fld_separate)
    placeholder = paragraph.add_run("The table of contents will be updated by Word.")
    placeholder.italic = True
    run._r.append(fld_end)


def add_page_number(paragraph) -> None:
    run = paragraph.add_run()
    fld_begin = OxmlElement("w:fldChar")
    fld_begin.set(qn("w:fldCharType"), "begin")
    instr = OxmlElement("w:instrText")
    instr.set(qn("xml:space"), "preserve")
    instr.text = "PAGE"
    fld_end = OxmlElement("w:fldChar")
    fld_end.set(qn("w:fldCharType"), "end")
    run._r.append(fld_begin)
    run._r.append(instr)
    run._r.append(fld_end)


def configure_document(doc: Document, title: str) -> None:
    section = doc.sections[0]
    section.top_margin = Cm(1.9)
    section.bottom_margin = Cm(1.9)
    section.left_margin = Cm(2.1)
    section.right_margin = Cm(2.1)

    normal = doc.styles["Normal"]
    normal.font.name = "Aptos"
    normal.font.size = Pt(10.5)
    normal.paragraph_format.space_after = Pt(6)

    title_style = doc.styles["Title"]
    title_style.font.name = "Aptos Display"
    title_style.font.size = Pt(24)
    title_style.font.color.rgb = RGBColor(0x2A, 0x4D, 0x69)

    for style_name, size, color in (
        ("Heading 1", 18, RGBColor(0x2A, 0x4D, 0x69)),
        ("Heading 2", 13.5, RGBColor(0x35, 0x50, 0x70)),
        ("Heading 3", 11.5, RGBColor(0x5C, 0x67, 0x7D)),
    ):
        style = doc.styles[style_name]
        style.font.name = "Aptos Display"
        style.font.size = Pt(size)
        style.font.color.rgb = color

    header = section.header.paragraphs[0]
    header.alignment = WD_ALIGN_PARAGRAPH.RIGHT
    header_run = header.add_run(title)
    header_run.font.name = "Aptos"
    header_run.font.size = Pt(8.5)

    footer = section.footer.paragraphs[0]
    footer.alignment = WD_ALIGN_PARAGRAPH.CENTER
    add_page_number(footer)


def add_cover(doc: Document, title: str, subtitle: str) -> None:
    title_paragraph = doc.add_paragraph(style="Title")
    title_paragraph.alignment = WD_ALIGN_PARAGRAPH.LEFT
    title_paragraph.add_run(title)

    subtitle_paragraph = doc.add_paragraph()
    subtitle_paragraph.alignment = WD_ALIGN_PARAGRAPH.LEFT
    subtitle_run = subtitle_paragraph.add_run(subtitle)
    subtitle_run.font.name = "Aptos"
    subtitle_run.font.size = Pt(12)
    subtitle_run.italic = True
    subtitle_run.font.color.rgb = RGBColor(0x55, 0x55, 0x55)

    meta_table = doc.add_table(rows=1, cols=1)
    meta_cell = meta_table.cell(0, 0)
    set_cell_shading(meta_cell, "EAF4F4")
    set_cell_margins(meta_cell)
    paragraph = meta_cell.paragraphs[0]
    paragraph.add_run("Document generated on ").bold = True
    paragraph.add_run(dt.datetime.now().strftime("%Y-%m-%d %H:%M"))
    paragraph.add_run("\nScope: ").bold = True
    paragraph.add_run("editor, architecture, generator, client API, framebuffer plugin, launch scripts")
    paragraph.add_run("\nTarget audience: ").bold = True
    paragraph.add_run("MFD authors, C++ integrators, and runtime teams")

    doc.add_page_break()


def add_heading(doc: Document, text: str, level: int) -> None:
    doc.add_heading(strip_inline_markdown(text), level=level)


def add_body_paragraph(doc: Document, text: str) -> None:
    paragraph = doc.add_paragraph()
    paragraph.alignment = WD_ALIGN_PARAGRAPH.LEFT
    paragraph.add_run(strip_inline_markdown(text))


def add_bullet(doc: Document, text: str) -> None:
    paragraph = doc.add_paragraph(style="List Bullet")
    paragraph.add_run(strip_inline_markdown(text))


def add_numbered(doc: Document, text: str) -> None:
    paragraph = doc.add_paragraph(style="List Number")
    paragraph.add_run(strip_inline_markdown(text))


def add_code_block(doc: Document, lines: list[str]) -> None:
    table = doc.add_table(rows=1, cols=1)
    cell = table.cell(0, 0)
    set_cell_shading(cell, "F1F5F9")
    set_cell_margins(cell, top=90, start=120, bottom=90, end=120)
    paragraph = cell.paragraphs[0]
    run = paragraph.add_run("\n".join(lines))
    run.font.name = "Consolas"
    run.font.size = Pt(8.8)


def add_note_block(doc: Document, lines: list[str]) -> None:
    table = doc.add_table(rows=1, cols=1)
    cell = table.cell(0, 0)
    set_cell_shading(cell, "EEF6FF")
    set_cell_margins(cell)
    paragraph = cell.paragraphs[0]
    run = paragraph.add_run(strip_inline_markdown(" ".join(lines)))
    run.font.name = "Aptos"
    run.font.size = Pt(10)


def add_markdown_table(doc: Document, lines: list[str]) -> None:
    if len(lines) < 2:
        for line in lines:
            add_body_paragraph(doc, line)
        return

    def split_row(line: str) -> list[str]:
        trimmed = line.strip()
        if trimmed.startswith("|"):
            trimmed = trimmed[1:]
        if trimmed.endswith("|"):
            trimmed = trimmed[:-1]
        return [strip_inline_markdown(cell.strip()) for cell in trimmed.split("|")]

    header = split_row(lines[0])
    rows = [split_row(line) for line in lines[2:]]
    table = doc.add_table(rows=1 + len(rows), cols=len(header))
    table.style = "Table Grid"

    for idx, value in enumerate(header):
        cell = table.cell(0, idx)
        cell.text = value
        set_cell_shading(cell, "DDEBF7")
        set_cell_margins(cell)
        for run in cell.paragraphs[0].runs:
            run.bold = True
            run.font.name = "Aptos"
            run.font.size = Pt(9.5)

    for row_idx, row in enumerate(rows, start=1):
        for col_idx, value in enumerate(row):
            cell = table.cell(row_idx, col_idx)
            cell.text = value
            set_cell_margins(cell)
            for run in cell.paragraphs[0].runs:
                run.font.name = "Aptos"
                run.font.size = Pt(9.5)


def add_image(doc: Document, image_path: Path, caption: str) -> None:
    paragraph = doc.add_paragraph()
    paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    paragraph.add_run().add_picture(str(image_path), width=Inches(6.0))
    caption_paragraph = doc.add_paragraph()
    caption_paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    caption_run = caption_paragraph.add_run(caption)
    caption_run.font.name = "Aptos"
    caption_run.font.size = Pt(9)
    caption_run.italic = True


def build_document(source_path: Path, output_path: Path, repo_root: Path) -> None:
    lines = source_path.read_text(encoding="utf-8").splitlines()
    title = next((line[2:].strip() for line in lines if line.startswith("# ")), "MFDStudio - Detailed User Guide")
    subtitle = next((line.split(":", 1)[1].strip() for line in lines if line.startswith("Subtitle:")), "")

    doc = Document()
    configure_document(doc, title)
    add_cover(doc, title, subtitle)

    paragraph_buffer: list[str] = []
    table_buffer: list[str] = []
    code_buffer: list[str] = []
    note_buffer: list[str] = []
    in_code_block = False
    code_fence = "```"
    source_dir = source_path.parent

    def flush_paragraph() -> None:
        if paragraph_buffer:
            add_body_paragraph(doc, " ".join(part.strip() for part in paragraph_buffer if part.strip()))
            paragraph_buffer.clear()

    def flush_table() -> None:
        if table_buffer:
            add_markdown_table(doc, table_buffer[:])
            table_buffer.clear()

    def flush_code() -> None:
        if code_buffer:
            add_code_block(doc, code_buffer[:])
            code_buffer.clear()

    def flush_note() -> None:
        if note_buffer:
            add_note_block(doc, note_buffer[:])
            note_buffer.clear()

    for index, line in enumerate(lines):
        if index == 0 and line.startswith("# "):
            continue
        if line.startswith("Subtitle:"):
            continue

        trimmed = line.strip()

        if in_code_block:
            if trimmed.startswith(code_fence):
                flush_code()
                in_code_block = False
            else:
                code_buffer.append(line)
            continue

        if trimmed.startswith(code_fence):
            flush_paragraph()
            flush_table()
            flush_note()
            in_code_block = True
            continue

        if trimmed.startswith("![") and "](" in trimmed and trimmed.endswith(")"):
            flush_paragraph()
            flush_table()
            flush_note()
            separator_index = trimmed.index("](")
            caption = trimmed[2:separator_index].strip()
            relative_image = trimmed[separator_index + 2:-1].strip()
            add_image(doc, (source_dir / relative_image).resolve(), caption)
            continue

        if trimmed.startswith("|"):
            flush_paragraph()
            flush_note()
            table_buffer.append(line)
            continue

        if table_buffer and not trimmed.startswith("|"):
            flush_table()

        if trimmed.startswith("> "):
            flush_paragraph()
            note_buffer.append(trimmed[2:].strip())
            continue

        if note_buffer and not trimmed.startswith("> "):
            flush_note()

        if not trimmed:
            flush_paragraph()
            flush_note()
            continue

        if trimmed == "[[TOC]]":
            toc_heading = doc.add_heading("Table of Contents", level=1)
            add_toc(doc.add_paragraph())
            doc.add_page_break()
            continue

        if trimmed == "<!-- PAGEBREAK -->":
            flush_paragraph()
            flush_note()
            doc.add_page_break()
            continue

        if trimmed.startswith("### "):
            flush_paragraph()
            flush_note()
            add_heading(doc, trimmed[4:].strip(), 3)
            continue

        if trimmed.startswith("## "):
            flush_paragraph()
            flush_note()
            add_heading(doc, trimmed[3:].strip(), 2)
            continue

        if trimmed.startswith("# "):
            flush_paragraph()
            flush_note()
            add_heading(doc, trimmed[2:].strip(), 1)
            continue

        if re.match(r"^\d+\.\s", trimmed):
            flush_paragraph()
            flush_note()
            add_numbered(doc, trimmed)
            continue

        if trimmed.startswith("- "):
            flush_paragraph()
            flush_note()
            add_bullet(doc, trimmed[2:].strip())
            continue

        paragraph_buffer.append(trimmed)

    flush_paragraph()
    flush_table()
    flush_note()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    doc.save(output_path)


def main() -> int:
    args = parse_args()
    repo_root = Path(__file__).resolve().parents[2]
    source_path = resolve_path(repo_root, args.source)
    output_path = resolve_path(repo_root, args.output)
    rendered_dir = resolve_path(repo_root, args.rendered_diagram_dir)

    if not args.skip_diagram_rendering:
        render_diagrams(repo_root, rendered_dir)

    build_document(source_path, output_path, repo_root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
