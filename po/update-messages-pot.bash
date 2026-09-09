#!/bin/bash

# Most i18n keywords were suggestions taken from KDE documentation.
#
# Link: https://develop.kde.org/docs/plasma/widget/translations-i18n/#generating-templatepot
xgettext \
	`find .. -name '*.js' -o -name '*.qml' -o -name '*.cpp'` \
	-ki18n:1 -ki18nc:1c,2 -ki18np:1,2 -ki18ncp:1c,2,3 -ktr2i18n:1 \
	-kI18N_NOOP:1 -kI18N_NOOP2:1c,2 -kN_:1 -kaliasLocale \
	-kki18n:1 -kki18nc:1c,2 -kki18np:1,2 -kki18ncp:1c,2,3 \
	-ki18nd:2 -ki18ndp:2,3 \
	--from-code UTF-8 \
	--copyright-holder='Anthon Open Source Community' \
	--package-name='plasma-aosc-updates' \
	--package-version='1.0' \
	--msgid-bugs-address='maintainers@aosc.io' \
	-o messages.pot
