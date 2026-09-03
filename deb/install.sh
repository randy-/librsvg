#!/bin/sh
#   
#   Copyright (C) 2026 Randy Butler
#
#   This program is free software; you can redistribute it and/or
#   modify it under the terms of the GNU Library General Public License as
#   published by the Free Software Foundation; either version 2 of the
#   License, or (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#   Library General Public License for more details.
#
#   You should have received a copy of the GNU Library General Public
#   License along with this program; if not, write to the
#   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
#   Boston, MA 02111-1307, USA.
#
# Install local librsvg-c debs in one apt transaction.
# Runtime set is c2+common+bin; -dev is optional.
# Default: sha256sum checksums, then apt-get. GPG is opt-in (--verify-gpg).
# Run as root from the directory that contains the .deb files
# (or invoke this script from that directory).
set -e
cd "$(dirname "$0")"

usage() {
	cat <<EOF
Usage: $0 [--verify-gpg]
  Install local librsvg-c debs (c2+common+bin; -dev optional).
  Default: sha256sum checksums, then apt-get.
  --verify-gpg   verify .asc with the published pubkey (temp GNUPGHOME)
  -h, --help     show this help
EOF
}

VERIFY_GPG=0
show_help=0
for arg in "$@"; do
	case $arg in
	-h|--help)
		show_help=1
		;;
	--verify-gpg)
		VERIFY_GPG=1
		;;
	*)
		echo "$0: unknown option: $arg" >&2
		usage >&2
		exit 1
		;;
	esac
done
if [ "$show_help" -eq 1 ]; then
	usage
	exit 0
fi

if [ "$(id -u)" -ne 0 ]; then
	echo "$0: run as root" >&2
	exit 1
fi

# Isolated GPG check. Does not write the user keyring.
# Missing -dev is not an error; its .asc is required only if the .deb is present.
GPGHOME=
gpg_verify() {
	file=$1
	if [ ! -f "${file}.asc" ]; then
		echo "$0: --verify-gpg: missing ${file}.asc" >&2
		exit 1
	fi
	gpg --homedir "$GPGHOME" --batch --trust-model always \
		--verify "${file}.asc" "$file"
}

verify_gpg() {
	if ! command -v gpg >/dev/null 2>&1; then
		echo "$0: --verify-gpg needs gpg" >&2
		exit 1
	fi
	pubkey=
	for p in oss-familybusinesssoftware.com-pubkey.asc \
		oss-familybusinesssoftware-pubkey.asc; do
		if [ -f "$p" ]; then
			pubkey=$p
			break
		fi
	done
	if [ -z "$pubkey" ]; then
		echo "$0: --verify-gpg: missing pubkey (oss-familybusinesssoftware.com-pubkey.asc)" >&2
		exit 1
	fi
	GPGHOME=$(mktemp -d)
	chmod 700 "$GPGHOME"
	trap 'rm -rf "$GPGHOME"' EXIT
	gpg --homedir "$GPGHOME" --batch --import "$pubkey"
	for pat in librsvg-c2_*.deb librsvg-c-common_*.deb librsvg-c-bin_*.deb \
		librsvg-c-dev_*.deb; do
		for f in $pat; do
			if [ -f "$f" ]; then
				gpg_verify "$f"
			fi
		done
	done
	if [ -f install.sh.asc ]; then
		gpg_verify install.sh
	fi
	if ls librsvg-c_*_amd64.SHA256SUMS >/dev/null 2>&1; then
		for s in librsvg-c_*_amd64.SHA256SUMS; do
			gpg_verify "$s"
		done
	elif [ -f SHA256SUMS ]; then
		gpg_verify SHA256SUMS
	fi
	rm -rf "$GPGHOME"
	GPGHOME=
	trap - EXIT
}

if [ "$VERIFY_GPG" -eq 1 ]; then
	verify_gpg
fi

# Verify checksums. Missing librsvg-c-dev_*.deb (and its .asc) is not
# an error. install.sh itself, if listed, must be present (we are running it).
verify_sums() {
	sums=$1
	if sha256sum --help 2>&1 | grep -q -- --ignore-missing; then
		sha256sum --ignore-missing -c "$sums"
		return
	fi
	have_dev=
	for f in librsvg-c-dev_*.deb; do
		if [ -f "$f" ]; then
			have_dev=1
			break
		fi
	done
	if [ -n "$have_dev" ]; then
		sha256sum -c "$sums"
		return
	fi
	filtered=$(mktemp)
	grep -v 'librsvg-c-dev_' "$sums" > "$filtered" || true
	if sha256sum -c "$filtered"; then
		rm -f "$filtered"
	else
		st=$?
		rm -f "$filtered"
		exit "$st"
	fi
}

if ls librsvg-c_*_amd64.SHA256SUMS >/dev/null 2>&1; then
	for s in librsvg-c_*_amd64.SHA256SUMS; do
		verify_sums "$s"
	done
elif [ -f SHA256SUMS ]; then
	verify_sums SHA256SUMS
fi

# c2 + common + bin are required on a desktop. -dev is optional.
need=
for pat in librsvg-c2_*.deb librsvg-c-common_*.deb librsvg-c-bin_*.deb; do
	found=
	for f in $pat; do
		if [ -f "$f" ]; then
			need="$need ./$f"
			found=1
		fi
	done
	if [ -z "$found" ]; then
		echo "$0: missing $pat" >&2
		exit 1
	fi
done
for f in librsvg-c-dev_*.deb; do
	if [ -f "$f" ]; then
		need="$need ./$f"
	fi
done

# shellcheck disable=SC2086
exec apt-get install -y $need
