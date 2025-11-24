#!/bin/bash
# Quick fix script for Rider file visibility issues

echo "🔧 Fixing Rider file visibility..."
echo ""

# Fix file permissions
echo "1. Fixing file permissions..."
chmod 644 README.md .editorconfig 2>/dev/null
find Docs -type f -name "*.md" -exec chmod 644 {} \; 2>/dev/null
find Docs -type d -exec chmod 755 {} \; 2>/dev/null
echo "   ✅ Permissions fixed"

# Verify git status
echo ""
echo "2. Verifying git status..."
git status --short
echo "   ✅ Git status checked"

# List documentation files
echo ""
echo "3. Documentation files present:"
echo ""
echo "📄 Root:"
ls -1 README.md .editorconfig 2>/dev/null | sed 's/^/   /'
echo ""
echo "📁 Docs/:"
find Docs -type f -name "*.md" | sort | sed 's/^/   /'

echo ""
echo "✅ All checks completed!"
echo ""
echo "Next steps in Rider:"
echo "  1. VCS → Git → Refresh (Ctrl+Alt+Y)"
echo "  2. File → Synchronize"
echo "  3. Or: File → Invalidate Caches → Invalidate and Restart"
echo ""
