"""Normalize official MCC kit XML dialects for reload evidence (not retail discovery)."""
import re
import xml.etree.ElementTree as ET
from pathlib import Path


def read(path):
    raw = Path(path).read_bytes()
    try:
        text = raw.decode("utf-8-sig")
    except UnicodeDecodeError:
        text = raw.decode("latin-1")
    # H4EK emits binary padding as XML text. Retain raw files and hashes; only
    # the parser view omits XML-forbidden controls, never meaningful tag data.
    text = re.sub(r"[\x00-\x08\x0b\x0c\x0e-\x1f]", "", text)
    def attribute(match):
        value = re.sub(r"&(?!amp;|lt;|gt;|quot;|apos;|#\d+;|#x[0-9a-fA-F]+;)", "&amp;", match[1])
        return '="' + value.replace("<", "&lt;").replace(">", "&gt;") + '"'
    text = re.sub(r'="([^"\r\n]*)"', attribute, text)
    root = ET.fromstring(text)

    def normalize(parent):
        children = list(parent)
        parent[:] = []
        active = None
        count = 0
        for child in children:
            normalize(child)
            if child.tag == "field" and child.get("type") == "block":
                if active is not None and count:
                    raise ValueError("incomplete official block " + active.get("name", ""))
                count = int(child.get("value"))
                active = ET.Element("block", name=child.get("name"))
                parent.append(active)
            elif child.tag == "element" and active is not None and count:
                active.append(child)
                count -= 1
            else:
                if count:
                    raise ValueError("unexpected field in official block")
                active = None
                parent.append(child)
        if count:
            raise ValueError("truncated official block")
    normalize(root)
    return root


def values(parent, name):
    return [(x.get("value") if x.get("value") is not None else (x.text or "").strip())
            for x in parent if x.get("name") == name and x.tag in ("field", "tag_reference")]


def value(parent, name, default=None):
    found = values(parent, name)
    if found:
        return found[0]
    if default is not None:
        return default
    raise ValueError("missing " + name)


def block(parent, name):
    return next((x for x in parent if x.tag == "block" and x.get("name") == name), [])


def descendant_blocks(parent, name):
    return [x for x in parent.iter("block") if x.get("name") == name]
