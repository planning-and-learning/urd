# Sphinx configuration for urd's documentation: build/docs-venv/bin/sphinx-build -b html docs build/docs/html
# (the virtual environment comes from docs/requirements.txt).

import sys
from pathlib import Path

# The request language pages are generated from share/urd/vocabulary.json (docs/_ext/urd_language.py).
sys.path.insert(0, str(Path(__file__).parent / "_ext"))

project = "urd"
author = "Dominik Drexler"
copyright = "2025-2026, Dominik Drexler"
release = "0.1.0"

extensions = ["myst_parser", "sphinx.ext.githubpages", "sphinx.ext.graphviz", "urd_language"]
# The category diagrams; Graphviz's `dot` must be on the path.
graphviz_output_format = "svg"
source_suffix = {".rst": "restructuredtext", ".md": "markdown"}
exclude_patterns = ["_build", "_ext", "SOURCES.json"]
myst_heading_anchors = 3

primary_domain = "cpp"
highlight_language = "cpp"
cpp_index_common_prefix = ["urd::", "urd::repr::"]

html_theme = "furo"
html_title = "urd"
html_theme_options = {
    "source_repository": "https://github.com/verdog-ai/urd/",
    "source_branch": "main",
    "source_directory": "docs/",
}
