#!/bin/sh
set -u

packages=${HAIKODE_DOCTOR_PACKAGES:-"haiku_devel gcc_syslibs_devel cmd:clang libgit2_1.9_devel lexilla_devel yaml_cpp0.8_devel editorconfig_core_c_devel openssl3_devel lsp_framework curl_devel"}
mode=${1:-check}

say() {
	printf '%s\n' "$*"
}

has_command() {
	command -v "$1" >/dev/null 2>&1
}

is_haiku() {
	[ "$(uname -s 2>/dev/null)" = "Haiku" ]
}

pkg_installed() {
	case "$1" in
		cmd:*)
			has_command "${1#cmd:}"
			;;
		*)
			for package_file in \
				/system/packages/"$1"-*.hpkg \
				/boot/system/packages/"$1"-*.hpkg \
				/home/config/packages/"$1"-*.hpkg \
				/boot/home/config/packages/"$1"-*.hpkg
			do
				[ -e "$package_file" ] && return 0
			done
			return 1
			;;
	esac
}

status_line() {
	name=$1
	state=$2
	detail=$3
	printf '  %-34s %s %s\n' "$name" "$state" "$detail"
}

install_packages() {
	say "Installing Haikode build dependencies:"
	say "  pkgman install $packages"
	exec pkgman install $packages
}

say "Haikode dependency doctor"
say
say "Required package command:"
say "  pkgman install $packages"
say

if ! is_haiku; then
	say "This dependency set is for Haiku. Run this on the Haiku computer before make."
	say "On Haiku, use:"
	say "  make doctor"
	say "  make install-deps"
	exit 0
fi

if ! has_command pkgman; then
	say "pkgman was not found. This does not look like a normal Haiku package-managed environment."
	exit 1
fi

if [ "$mode" = "--install" ]; then
	install_packages
fi

missing_packages=""
say "Package status:"
for package in $packages; do
	if pkg_installed "$package"; then
		status_line "$package" "ok" "installed"
	else
		status_line "$package" "missing" "install with pkgman"
		missing_packages="$missing_packages $package"
	fi
done

say
say "Tool status:"
missing_tools=0
for tool in make g++ getarch findpaths rc; do
	if has_command "$tool"; then
		status_line "$tool" "ok" "$(command -v "$tool")"
	else
		status_line "$tool" "missing" "needed for a complete build"
		missing_tools=1
	fi
done

say
if [ -n "$missing_packages" ]; then
	say "Missing packages detected."
	say "Run:"
	say "  make install-deps"
	say
	say "Or directly:"
	say "  pkgman install$missing_packages"
	exit 1
fi

if [ "$missing_tools" -ne 0 ]; then
	say "Some build tools are missing even though packages may be installed."
	say "Run make install-deps first, then re-run make doctor."
	exit 1
fi

say "Dependency check passed. You can run:"
say "  make"
