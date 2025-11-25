# GAS Migration Correction Pipeline

**Date**: 2025-11-25
**Issue**: Critical inheritance errors in GAS migration

---

## 🔴 Проблемы Найдены

### 1. Effects с неправильным базовым классом (5 файлов)
```cpp
// ❌ НЕПРАВИЛЬНО:
class GAS_API UGameplayEffect_HealthRegen : public UMedComGameplayEffect

// ✅ ПРАВИЛЬНО:
class GAS_API UGameplayEffect_HealthRegen : public UGASEffect
```

**Файлы для исправления:**
- `GameplayEffect_StaminaRegen.h` - `UMedComGameplayEffect` → `UGASEffect`
- `GameplayEffect_JumpCost.h` - `UMedComGameplayEffect` → `UGASEffect`
- `GameplayEffect_HealthRegen.h` - `UMedComGameplayEffect` → `UGASEffect`
- `InitialAttributesEffect.h` - `UMedComGameplayEffect` → `UGASEffect`
- `GameplayEffect_SprintCost.h` - `UMedComGameplayEffect` → `UGASEffect`
- `GameplayEffect_SprintBuff.h` - проверить (возможно тоже)
- `GameplayEffect_CrouchDebuff.h` - `UGameplayEffect` → `UGASEffect`

### 2. Character Abilities с неправильным базовым классом (3 файла)
```cpp
// ❌ НЕПРАВИЛЬНО:
class GAS_API UCharacterSprintAbility : public UGameplayAbility

// ✅ ПРАВИЛЬНО:
class GAS_API UCharacterSprintAbility : public UGASAbility
```

**Файлы для исправления:**
- `CharacterSprintAbility.h` - `UGameplayAbility` → `UGASAbility`
- `CharacterJumpAbility.h` - `UGameplayAbility` → `UGASAbility`
- `CharacterCrouchAbility.h` - `UGameplayAbility` → `UGASAbility`

### 3. Attribute Sets с неправильным базовым классом (3 файла)
```cpp
// ❌ НЕПРАВИЛЬНО:
class GAS_API UAmmoAttributeSet : public UAttributeSet

// ✅ ПРАВИЛЬНО:
class GAS_API UAmmoAttributeSet : public UGASAttributeSet
```

**Файлы для исправления:**
- `AmmoAttributeSet.h` - `UAttributeSet` → `UGASAttributeSet`
- `WeaponAttributeSet.h` - `UAttributeSet` → `UGASAttributeSet`
- `ArmorAttributeSet.h` - `UAttributeSet` → `UGASAttributeSet`

---

## 🔧 Plan Исправления

### Step 1: Fix Effects Inheritance
```bash
# Fix all effects to inherit from UGASEffect
find Source/GAS/Public/Effects -name "*.h" -exec sed -i 's/: public UMedComGameplayEffect/: public UGASEffect/g' {} \;
find Source/GAS/Public/Effects -name "*.h" -exec sed -i 's/: public UGameplayEffect\([^S]\)/: public UGASEffect\1/g' {} \;
```

### Step 2: Fix Character Abilities Inheritance
```bash
# Fix character abilities to inherit from UGASAbility
sed -i 's/class GAS_API UCharacterSprintAbility : public UGameplayAbility/class GAS_API UCharacterSprintAbility : public UGASAbility/g' Source/GAS/Public/Abilities/CharacterSprintAbility.h
sed -i 's/class GAS_API UCharacterJumpAbility : public UGameplayAbility/class GAS_API UCharacterJumpAbility : public UGASAbility/g' Source/GAS/Public/Abilities/CharacterJumpAbility.h
sed -i 's/class GAS_API UCharacterCrouchAbility : public UGameplayAbility/class GAS_API UCharacterCrouchAbility : public UGASAbility/g' Source/GAS/Public/Abilities/CharacterCrouchAbility.h
```

### Step 3: Fix Attribute Sets Inheritance
```bash
# Fix attribute sets to inherit from UGASAttributeSet (only for non-base AttributeSets)
sed -i 's/class GAS_API UAmmoAttributeSet : public UAttributeSet/class GAS_API UAmmoAttributeSet : public UGASAttributeSet/g' Source/GAS/Public/Attributes/AmmoAttributeSet.h
sed -i 's/class GAS_API UWeaponAttributeSet : public UAttributeSet/class GAS_API UWeaponAttributeSet : public UGASAttributeSet/g' Source/GAS/Public/Attributes/WeaponAttributeSet.h
sed -i 's/class GAS_API UArmorAttributeSet : public UAttributeSet/class GAS_API UArmorAttributeSet : public UGASAttributeSet/g' Source/GAS/Public/Attributes/ArmorAttributeSet.h
```

### Step 4: Verify All Changes
```bash
# Verify no UMedCom* references remain in GAS headers
grep -r "UMedCom" Source/GAS/Public --include="*.h"

# Should return NO results (or only in comments)
```

### Step 5: Check includes are correct
```bash
# Verify GASEffect.h is included where needed
grep -l "UGASEffect" Source/GAS/Public/Effects/*.h | while read f; do
  grep -q "GASEffect.h" "$f" || echo "Missing include in $f"
done

# Verify GASAbility.h is included where needed
grep -l "UGASAbility" Source/GAS/Public/Abilities/*.h | while read f; do
  grep -q "GASAbility.h" "$f" || echo "Missing include in $f"
done

# Verify GASAttributeSet.h is included where needed
grep -l "UGASAttributeSet" Source/GAS/Public/Attributes/*.h | while read f; do
  grep -q "GASAttributeSet.h" "$f" || echo "Missing include in $f"
done
```

---

## ✅ Verification Checklist

After corrections:

- [ ] No `UMedComGameplayEffect` references in GAS module
- [ ] All effects inherit from `UGASEffect`
- [ ] All character abilities inherit from `UGASAbility`
- [ ] All attribute sets (except base) inherit from `UGASAttributeSet`
- [ ] All includes are correct (`GASEffect.h`, `GASAbility.h`, `GASAttributeSet.h`)
- [ ] No compilation errors

---

## 📝 Root Cause Analysis

**Why did this happen?**

The migration script used this sed pattern:
```bash
sed -i 's/UMedComGameplayEffect\([^_]\)/UGASEffect\1/g' "$dst_file"
```

**Problem**: The pattern `\([^_]\)` requires a character after the class name, but when the class name appears at END of line (like `: public UMedComGameplayEffect`), there's no character after it, so the pattern didn't match!

**Fix**: Use `\([^_]\|$\)` to match either non-underscore OR end-of-line.

---

## 🎯 Improved Migration Pattern (for future)

```bash
# Better pattern that works at end of line:
sed -i 's/UMedComGameplayEffect\($\|[^_]\)/UGASEffect\1/g' "$dst_file"
```

This matches `UMedComGameplayEffect` followed by either:
- End of line (`$`)
- A character that's not underscore (`[^_]`)

---

**Status**: Ready to execute corrections
**Impact**: CRITICAL - must fix before compilation
**Estimated Time**: 5 minutes
