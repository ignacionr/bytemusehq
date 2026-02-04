#!/usr/bin/env bash
set -euo pipefail

# Check for uncommitted changes
if ! git diff-index --quiet HEAD --; then
    echo "Error: You have uncommitted changes. Please commit or stash them first."
    exit 1
fi

FLAKE_FILE="flake.nix"

# Extract current version from flake.nix
CURRENT_VERSION=$(sed -n 's/.*version = "\([0-9]*\.[0-9]*\.[0-9]*\)".*/\1/p' "$FLAKE_FILE")

if [[ -z "$CURRENT_VERSION" ]]; then
    echo "Error: Could not find version in $FLAKE_FILE"
    exit 1
fi

echo "Current version: $CURRENT_VERSION"

# Split version into parts
IFS='.' read -r MAJOR MINOR PATCH <<< "$CURRENT_VERSION"

# Increment patch version
NEW_PATCH=$((PATCH + 1))
NEW_VERSION="${MAJOR}.${MINOR}.${NEW_PATCH}"

echo "New version: $NEW_VERSION"

# Update flake.nix with new version
sed -i.bak "s/version = \"$CURRENT_VERSION\"/version = \"$NEW_VERSION\"/" "$FLAKE_FILE"
rm -f "${FLAKE_FILE}.bak"

echo "Updated $FLAKE_FILE"

# Commit the change
git add "$FLAKE_FILE"
git commit -m "Bump version to $NEW_VERSION"

echo "Committed version bump"

# Create tag
TAG="v$NEW_VERSION"
git tag "$TAG"

echo "Created tag: $TAG"

# Push the commit and tag to origin
git push origin HEAD
git push origin "$TAG"

echo "Pushed tag $TAG to origin"
echo "Done! Version bumped from $CURRENT_VERSION to $NEW_VERSION"
