# Sphinx configuration for urd's documentation: build/docs-venv/bin/sphinx-build -b html docs build/docs/html
# (the virtual environment comes from docs/requirements.txt).

project = "urd"
author = "Dominik Drexler"
copyright = "2025-2026, Dominik Drexler"
release = "0.1.0"

extensions = ["myst_parser", "sphinx.ext.githubpages"]
source_suffix = {".rst": "restructuredtext", ".md": "markdown"}
exclude_patterns = ["_build", "SOURCES.json"]
myst_heading_anchors = 3

primary_domain = "cpp"
highlight_language = "cpp"
cpp_index_common_prefix = ["urd::", "urd::repr::", "urd::compose::"]

html_theme = "furo"
html_title = "urd"
html_theme_options = {
    "source_repository": "https://github.com/planning-and-learning/urd/",
    "source_branch": "main",
    "source_directory": "docs/",
}
