#!/bin/bash
# Quick script to verify and list all documentation files

echo "=== SuspenseCore Documentation Files ==="
echo ""
echo "Current branch: $(git branch --show-current)"
echo "Last commit: $(git log -1 --oneline)"
echo ""
echo "Documentation files:"
find Docs -type f -name "*.md" | sort
echo ""
echo "Root documentation:"
ls -1 README.md .editorconfig 2>/dev/null || echo "Root files missing!"
echo ""
echo "All files tracked in git:"
git ls-files | grep -E "(README|Docs|.editorconfig)"
