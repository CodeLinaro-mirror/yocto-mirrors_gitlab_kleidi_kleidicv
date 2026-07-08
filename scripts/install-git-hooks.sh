#!/usr/bin/env -S LC_ALL=C LANG=C bash

# SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

readonly WRAPPER_MARKER="# KleidiCV commit-msg wrapper"
readonly KLEIDICV_COMMIT_MSG="commit-msg.kleidicv"
readonly PRESERVED_COMMIT_MSG="commit-msg.preserved"
readonly GERRIT_COMMIT_MSG="commit-msg.gerrit"

cd "$(dirname "${BASH_SOURCE[0]}")/.."

if hooks_path="$(git config --get core.hooksPath)"; then
  cat >&2 <<EOF
ERROR: core.hooksPath is set to '${hooks_path}'.

Git will not run hooks from .git/hooks while core.hooksPath is set. Unset it
before installing KleidiCV hooks so an existing commit-msg hook can be
preserved and chained.
EOF
  exit 1
fi

hooks_dir="$(git rev-parse --git-path hooks)"
mkdir -p "${hooks_dir}"

install -m 755 .githooks/pre-commit "${hooks_dir}/pre-commit"
install -m 755 .githooks/commit-msg "${hooks_dir}/${KLEIDICV_COMMIT_MSG}"

commit_msg_hook="${hooks_dir}/commit-msg"
preserved_hook="${hooks_dir}/${PRESERVED_COMMIT_MSG}"
gerrit_hook="${hooks_dir}/${GERRIT_COMMIT_MSG}"
preserved_existing_hook=false

if [[ -f "${commit_msg_hook}" ]] &&
   ! grep -qF "${WRAPPER_MARKER}" "${commit_msg_hook}"; then
  if [[ -e "${preserved_hook}" ]] &&
     ! cmp -s "${commit_msg_hook}" "${preserved_hook}"; then
    cat >&2 <<EOF
ERROR: both ${commit_msg_hook} and ${preserved_hook} exist and differ.

Move the hook that should run before KleidiCV's checks to ${preserved_hook},
then rerun this script.
EOF
    exit 1
  fi

  if [[ ! -e "${preserved_hook}" ]]; then
    mv "${commit_msg_hook}" "${preserved_hook}"
    preserved_existing_hook=true
  fi
fi

cat >"${commit_msg_hook}" <<'EOF'
#!/usr/bin/env -S LC_ALL=C LANG=C bash
# KleidiCV commit-msg wrapper

set -euo pipefail

hook_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
preserved_hook="${hook_dir}/commit-msg.preserved"
gerrit_hook="${hook_dir}/commit-msg.gerrit"
kleidicv_hook="${hook_dir}/commit-msg.kleidicv"

if [[ -x "${preserved_hook}" ]]; then
  "${preserved_hook}" "$@"
fi

if [[ -x "${gerrit_hook}" ]]; then
  "${gerrit_hook}" "$@"
fi

"${kleidicv_hook}" "$@"
EOF

chmod 755 "${commit_msg_hook}"

echo "Installed KleidiCV Git hooks in ${hooks_dir}."
if [[ "${preserved_existing_hook}" == true ]]; then
  echo "Preserved existing commit-msg hook as ${preserved_hook}."
elif [[ -x "${preserved_hook}" ]]; then
  echo "Using preserved commit-msg hook from ${preserved_hook}."
fi
if [[ -x "${gerrit_hook}" ]]; then
  echo "Using Gerrit commit-msg hook from ${gerrit_hook}."
fi
