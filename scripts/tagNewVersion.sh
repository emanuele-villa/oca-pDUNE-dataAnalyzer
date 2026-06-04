#!/bin/bash
# Tag a new version of oca-pDUNE-dataAnalyzer.
echo "Script $0 started"

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
REPO_DIR="$( cd "${SCRIPT_DIR}/.." && pwd )"

print_help() {
    echo "*****************************************************************************"
    echo "Usage: $0 -v <version> -d <description> [-h]"
    echo "  -v | --version      version tag, e.g. v1.0"
    echo "  -d | --description  short release description (quoted string)"
    echo "  -h | --help         print this help message"
    echo "*****************************************************************************"
    exit 0
}

versionNumber=""
description=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        -v|--version)     versionNumber="$2"; shift 2 ;;
        -d|--description) description="$2";   shift 2 ;;
        -h|--help)        print_help ;;
        *) shift ;;
    esac
done

if [ -z "$versionNumber" ] || [ -z "$description" ]; then
    echo "ERROR: both -v and -d are required."
    print_help
fi

cd "$REPO_DIR"

# Warn if not on master
currentBranch=$(git branch --show-current)
if [ "$currentBranch" != "master" ]; then
    echo "WARNING: you are on branch '$currentBranch', not 'master'."
    read -p "Continue anyway? [y/N] " confirm
    [[ "$confirm" =~ ^[Yy]$ ]] || { echo "Aborted."; exit 1; }
fi

# Make sure we are up to date
git pull

# Check nothing is uncommitted
if ! git diff --quiet || ! git diff --cached --quiet; then
    echo "ERROR: there are uncommitted changes. Please commit or stash them first."
    exit 1
fi

# Write version file
echo "$versionNumber" > docs/version.txt
git add docs/version.txt
git commit -m "Release $versionNumber"
git push

# Create annotated tag
git tag -a "$versionNumber" -m "$description"
git push origin "$versionNumber"

echo "Tagged $versionNumber successfully: \"$description\""
exit 0
