#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo_root"

run_make() {
	set +e
	output=$(make "$@" 2>&1)
	status=$?
	set -e
	printf '%s' "$output"
	return "$status"
}

if ! doctor_output=$(run_make doctor); then
	printf '%s\n' "$doctor_output"
	printf 'make doctor should run before normal build dependency discovery\n' >&2
	exit 1
fi
case "$doctor_output" in
	*"makefile-engine"*|*"findpaths:"*|*"getarch:"*)
		printf '%s\n' "$doctor_output"
		printf 'doctor must not require Haiku makefile-engine discovery\n' >&2
		exit 1
		;;
esac
case "$doctor_output" in
	*"Haikode dependency doctor"*|*"pkgman install"*) ;;
	*)
		printf '%s\n' "$doctor_output"
		printf 'doctor output should explain dependency status and pkgman install hints\n' >&2
		exit 1
		;;
esac

if ! install_plan=$(run_make -n install-deps); then
	printf '%s\n' "$install_plan"
	printf 'make -n install-deps should show the dependency install command\n' >&2
	exit 1
fi
case "$install_plan" in
	*"pkgman install"*"libgit2"*"curl"*) ;;
	*)
		printf '%s\n' "$install_plan"
		printf 'install-deps dry-run should show the pkgman dependency command\n' >&2
		exit 1
		;;
esac
case "$install_plan" in
	*"makefile-engine"*|*"findpaths:"*|*"getarch:"*)
		printf '%s\n' "$install_plan"
		printf 'install-deps must not require Haiku makefile-engine discovery\n' >&2
		exit 1
		;;
esac

if [ "$(uname -s 2>/dev/null)" != "Haiku" ]; then
	if default_plan=$(run_make -n HAIKODE_AI_NETWORK=1); then
		printf '%s\n' "$default_plan"
		printf 'default make should still enter the native Haiku build path\n' >&2
		exit 1
	fi
	case "$default_plan" in
		*"makefile-engine"*|*"findpaths:"*|*"getarch:"*) ;;
		*)
			printf '%s\n' "$default_plan"
			printf 'default make should fail here only because this is not a Haiku build environment\n' >&2
			exit 1
			;;
	esac
fi

printf 'make-doctor-smoke-ok\n'
