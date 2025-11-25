# Coding Standards & Guidelines

Project-wide standards, conventions, and guidelines for the Suspense project.

## Contents

- **SuspenseNamingConvention.md** - Naming conventions (MedCom → Suspense)
- **ModuleStructureGuidelines.md** - Module structure and organization rules

## Purpose

This directory defines:
- Naming conventions for classes, files, and modules
- Code organization and structure standards
- Module layout requirements
- Best practices and patterns

## Key Rules

### Naming Convention
- Classes: `MedCom*` → `Suspense*`
- API Macros: `MEDCOM*_API` → `SUSPENSE*_API`
- Log Categories: `LogMedCom*` → `LogSuspense*`
- Blueprint Categories: `MedCom|*` → `Suspense|*`

### Module Structure
- Files go DIRECTLY in `System/Private/` and `System/Public/`
- DO NOT create nested `Suspense{Module}/` folders
- Build.cs goes in root of system directory
- Use subdirectories for organization (Components/, Utils/, etc.)

## Usage

**ALWAYS** consult these documents before:
- Creating new modules
- Migrating legacy code
- Refactoring existing code
- Adding new features

## Related Documentation

- **Planning/ModuleDesign.md**: Design principles
- **Migration/**: Migration procedures
