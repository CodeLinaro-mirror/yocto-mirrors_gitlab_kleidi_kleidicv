#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2023 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

# This is a helper script for developers to check and automate inclusion of
# correct license headers in their commits. It relies on `reuse` tool available
# at https://reuse.software
#
# The script is meant to be run before committing changes (after `git add`,
# before `git commit`) and can be turned into a pre-commit hook . It would check
# staging files for correct Arm copyright and amend copyright if needed. In case
# of outdated copyright year - extends the copyright.
# For non-recognisable file formats, such as .patch it would enforce c-style
# comment - it is recommended to check amended file to make sure this style is
# compatible with the file type.
# If any changes has been made as a result of running this script the user will
# be asked to add changes to commit.
# Script return values:
#   0 - default, nothing was changed
#   1 - reuse lint complains, but files haven't been modified
#   2 - files have been modified by the script and have to be staged for commit
#   3 - both 1 and 2

set -u

GREEN='\033[0;32m'
NC='\033[0m' # No Color

COPYRIGHT="Arm Limited and/or its affiliates <open-source-office@arm.com>"
LICENSE="Apache-2.0"
: "${STAGED_ONLY:=OFF}"

has_license_header() {
    local file=$1
    local license_tag="SPDX-License-""Identifier"

    grep -Fq "${COPYRIGHT}" "${file}" && grep -Fq "${license_tag}: ${LICENSE}" "${file}"
}

update_copyright_year() {
    local file=$1
    local current_year
    local tmp

    current_year=$(date +%Y)
    tmp=$(mktemp)

    awk -v copyright="${COPYRIGHT}" -v current_year="${current_year}" '
        index($0, "SPDX-FileCopyrightText:") && index($0, copyright) {
            prefix = $0
            sub(/SPDX-FileCopyrightText:.*/, "", prefix)
            rest = $0
            sub(/^.*SPDX-FileCopyrightText: /, "", rest)
            copyright_pos = index(rest, copyright)
            years = substr(rest, 1, copyright_pos - 2)
            split(years, range, /[[:space:]]*-[[:space:]]*/)
            if (range[1] == current_year) {
                $0 = prefix "SPDX-FileCopyrightText: " current_year " " copyright
            } else {
                $0 = prefix "SPDX-FileCopyrightText: " range[1] " - " current_year " " copyright
            }
        }
        { print }
    ' "${file}" > "${tmp}" && cat "${tmp}" > "${file}"
    local exit_code=$?

    rm -f "${tmp}"
    return "${exit_code}"
}

# Use `reuse` to check for copyrights.
# Suppresses `reuse` stderr not to spam console with `reuse -h` prompts for
# unrecognised formats.
annotate() {
    local file=$1
    local out
    local exit_code
    local annotate_args=(-c "${COPYRIGHT}" --license "${LICENSE}" --merge-copyrights)

    if has_license_header "${file}"; then
        update_copyright_year "${file}"
        return $?
    fi

    out=$(reuse annotate "${annotate_args[@]}" "${file}" 2>&1)
    exit_code=$?

    if [ "${exit_code}" -ne 2 ]; then
        echo "${out}"
        return "${exit_code}"
    fi

    out=$(reuse annotate "${annotate_args[@]}" --style c "${file}" 2>&1)
    exit_code=$?

    if [ "${exit_code}" -eq 0 ]; then
        echo "${out}"
        return 0
    fi

    if has_license_header "${file}"; then
        update_copyright_year "${file}"
        return $?
    fi

    echo "${out}"
    return "${exit_code}"
}

UNSTAGED=$(git diff --name-only)
mapfile -d '' STAGED < <(git diff --cached --name-only --diff-filter=ACMR -z)
EXIT_CODE=0
for file in "${STAGED[@]}"; do
    if ! annotate "${file}"; then
        EXIT_CODE=1
    fi
done

if [[ "${STAGED_ONLY}" != "ON" ]]; then
    # Run license check on the entire codebase.
    reuse lint
    EXIT_CODE=$((EXIT_CODE | $?))
fi

# Notify user if there were changes made to staging files
for file in $(git diff --name-only); do
    if ! echo "${UNSTAGED}" | grep -Fxq -- "$file"; then
        echo -e "${GREEN}Please stage ${file} for the commit${NC}"
        EXIT_CODE=$((EXIT_CODE | 2))
    fi
done

exit "${EXIT_CODE}"
