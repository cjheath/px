#
# Build/serve this gh-pages site locally with Jekyll, the same way GitHub Pages
# renders it in production (the Gemfile pins the "github-pages" gem for that reason).
#
# First time:    make deps
# While editing: make serve   (rebuilds on change, http://HOST:PORT)
# One-off:       make build   (output in _site/)
#
# --livereload opens a SECOND port (LIVEPORT) for its websocket, separate from
# the main PORT jekyll serves on - "no acceptor (port is in use or requires
# root privileges)" usually means one of the two is already taken, not that
# it needs root (neither default, 4000 or 35729, is privileged). Override
# either, e.g.: make serve PORT=4001 LIVEPORT=35730
# If it still won't bind, try `make serve-plain` (no livereload, one port only).

HOST     = 127.0.0.1
PORT     = 4400
LIVEPORT = 35929

# Rouge (kramdown's code highlighter, configured in _config.yml) wraps tokens in
# <span class="k">, <span class="s"> etc. at build time - same locally as on GitHub -
# but supplies no colours of its own. rouge-theme renders one of Rouge's built-in
# themes to assets/rouge.css, which the layout links; re-run after changing THEME.
# `bundle exec rougify help style` lists every theme name.
THEME = github

.PHONY: deps serve serve-plain build clean rouge-theme

serve:
	bundle exec jekyll serve --livereload --host $(HOST) --port $(PORT) --livereload-port $(LIVEPORT)

serve-plain:
	bundle exec jekyll serve --host $(HOST) --port $(PORT)

deps:
	bundle install

rouge-theme:
	bundle exec rougify style $(THEME) > assets/rouge.css

build:
	bundle exec jekyll build

clean:
	rm -rf _site .jekyll-cache
