# tabbed - tabbing interface
# See LICENSE file for copyright and license details.

include config.mk

SRC = tabbed.c xembed.c
EXTRA_OBJ = zmq_server.c
OBJ = ${SRC:.c=.o} ${EXTRA_OBJ}
BIN = ${OBJ:.o=}

all: options ${BIN}

options:
	@echo tabbed build options:
	@echo "CFLAGS     = ${CFLAGS}"
	@echo "LDFLAGS    = ${LDFLAGS}"
	@echo "CC         = ${CC}"
	@echo "CC version = `${CC} --version`"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk

config.h:
	@echo creating $@ from config.def.h
	@cp config.def.h $@

.o:
	@echo ${CC} -o $@ ${EXTRA_OBJ} $< ${LDFLAGS}
	@${CC} -o $@ ${EXTRA_OBJ} $< ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f ${BIN} ${OBJ} tabbed-${VERSION}.tar.gz

dist: clean
	@echo creating dist tarball
	@mkdir -p tabbed-${VERSION}
	@cp -R LICENSE Makefile README config.def.h config.mk \
		tabbed.1 arg.h ${SRC} tabbed-${VERSION}
	@tar -cf tabbed-${VERSION}.tar tabbed-${VERSION}
	@gzip tabbed-${VERSION}.tar
	@rm -rf tabbed-${VERSION}

install: all
	@echo installing executable files to ${DESTDIR}${PREFIX}/bin
	@mkdir -p "${DESTDIR}${PREFIX}/bin"
	@cp -f ${BIN} "${DESTDIR}${PREFIX}/bin"
	@chmod 755 "${DESTDIR}${PREFIX}/bin/tabbed"
	@echo installing manual pages to ${DESTDIR}${MANPREFIX}/man1
	@mkdir -p "${DESTDIR}${MANPREFIX}/man1"
	@sed "s/VERSION/${VERSION}/g" < tabbed.1 > "${DESTDIR}${MANPREFIX}/man1/tabbed.1"
	@chmod 644 "${DESTDIR}${MANPREFIX}/man1/tabbed.1"
	@sed "s/VERSION/${VERSION}/g" < xembed.1 > "${DESTDIR}${MANPREFIX}/man1/xembed.1"
	@chmod 644 "${DESTDIR}${MANPREFIX}/man1/xembed.1"
ifdef BUILD_INSTRUMENTED_COVERAGE
	@mkdir -p "${DESTDIR}${GCNOPREFIX}"
	@echo installing gcov files
	find . -name "*.gcno" -exec mv {} "${DESTDIR}${GCNOPREFIX}" \;
endif

uninstall:
	@echo removing executable files from ${DESTDIR}${PREFIX}/bin
	@rm -f "${DESTDIR}${PREFIX}/bin/tabbed"
	@rm -f "${DESTDIR}${PREFIX}/bin/xembed"
	@echo removing manual pages from ${DESTDIR}${MANPREFIX}/man1
	@rm -f "${DESTDIR}${MANPREFIX}/man1/tabbed.1"
	@rm -f "${DESTDIR}${MANPREFIX}/man1/xembed.1"
ifdef BUILD_INSTRUMENTED_COVERAGE
	@echo installing gcov files
	find "${DESTDIR}${GCNOPREFIX}" -name "*.gcno" -exec rm {} \;
endif

.PHONY: all options clean dist install uninstall
