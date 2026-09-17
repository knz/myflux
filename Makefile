# myflux - nightly colour temperature and greyscale orchestration.
#
#   make            build the two C helpers
#   make install    symlink everything into ~/bin and enable the user service
#   make uninstall  remove those symlinks (leaves this directory alone)

CFLAGS  ?= -O2 -Wall
LDLIBS   = -lX11 -lXrandr

BINDIR   = $(HOME)/bin
UNITDIR  = $(HOME)/.config/systemd/user
AUTODIR  = $(HOME)/.config/autostart

PROGS    = ctmset ctmget
SCRIPTS  = myfluxd myfluxctl myfluxtray
LOCS     = $(wildcard myflux-*)   # note: myfluxtray has no dash, so it is not a location
LINKS    = $(PROGS) $(SCRIPTS) $(LOCS)

all: $(PROGS)

ctmset: ctmset.c
ctmget: ctmget.c

install: all
	mkdir -p $(BINDIR) $(UNITDIR)
	for f in $(LINKS); do ln -sfn $(CURDIR)/$$f $(BINDIR)/$$f; done
	ln -sfn $(CURDIR)/myfluxd.service $(UNITDIR)/myfluxd.service
	mkdir -p $(AUTODIR)
	sed 's|@BIN@|$(BINDIR)|' myfluxtray.desktop.in > $(AUTODIR)/myfluxtray.desktop
	systemctl --user daemon-reload
	@echo
	@echo "Now:  systemctl --user enable --now myfluxd.service"
	@echo "Tray: myfluxtray &   (autostarts on next login)"
	@echo "And:  myfluxctl loc <ams|nyc|tok|toronto>"

uninstall:
	-systemctl --user disable --now myfluxd.service
	for f in $(LINKS); do rm -f $(BINDIR)/$$f; done
	rm -f $(UNITDIR)/myfluxd.service $(AUTODIR)/myfluxtray.desktop
	systemctl --user daemon-reload

clean:
	rm -f $(PROGS)

.PHONY: all install uninstall clean
