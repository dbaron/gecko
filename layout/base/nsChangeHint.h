/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* constants for what needs to be recomputed in response to style changes */

#ifndef nsChangeHint_h___
#define nsChangeHint_h___

#include "mozilla/TypedEnumBits.h"
#include "mozilla/Types.h"
#include "nsDebug.h"
#include "nsTArray.h"
#include "mozilla/ServoStyleConsts.h"

// Defines for various style related constants

enum class nsChangeHint : uint32_t {
  Empty = 0,

  // change was visual only (e.g., COLOR=)
  // Invalidates all descendant frames (including following
  // placeholders to out-of-flow frames).
  RepaintFrame = 1 << 0,

  // For reflow, we want flags to give us arbitrary FrameNeedsReflow behavior.
  // just do a FrameNeedsReflow.
  NeedReflow = 1 << 1,

  // Invalidate intrinsic widths on the frame's ancestors.  Must not be set
  // without setting NeedReflow.
  ClearAncestorIntrinsics = 1 << 2,

  // Invalidate intrinsic widths on the frame's descendants.  Must not be set
  // without also setting ClearAncestorIntrinsics,
  // NeedDirtyReflow and nsChangeHint::NeedReflow.
  ClearDescendantIntrinsics = 1 << 3,

  // Force unconditional reflow of all descendants.  Must not be set without
  // setting NeedReflow, but can be set regardless of whether the
  // Clear*Intrinsics flags are set.
  NeedDirtyReflow = 1 << 4,

  // change requires view to be updated, if there is one (e.g., clip:).
  // Updates all descendants (including following placeholders to out-of-flows).
  SyncFrameView = 1 << 5,

  // The currently shown mouse cursor needs to be updated
  UpdateCursor = 1 << 6,

  /**
   * Used when the computed value (a URI) of one or more of an element's
   * filter/mask/clip/etc CSS properties changes, causing the element's frame
   * to start/stop referencing (or reference different) SVG resource elements.
   * (_Not_ used to handle changes to referenced resource elements.) Using this
   * hint results in SVGObserverUtils::UpdateEffects being called on the
   * element's frame.
   */
  UpdateEffects = 1 << 7,

  /**
   * Visual change only, but the change can be handled entirely by
   * updating the layer(s) for the frame.
   * Updates all descendants (including following placeholders to out-of-flows).
   */
  UpdateOpacityLayer = 1 << 8,
  /**
   * Updates all descendants. Any placeholder descendants' out-of-flows
   * are also descendants of the transformed frame, so they're updated.
   */
  UpdateTransformLayer = 1 << 9,

  /**
   * Change requires frame change (e.g., display:).
   * Reconstructs all frame descendants, including following placeholders
   * to out-of-flows.
   *
   * Note that this subsumes all the other change hints. (see
   * RestyleManager::ProcessRestyledFrames for details).
   */
  ReconstructFrame = 1 << 10,

  /**
   * The frame's overflow area has changed. Does not update any descendant
   * frames.
   */
  UpdateOverflow = 1 << 11,

  /**
   * The overflow area of the frame and all of its descendants has changed. This
   * can happen through a text-decoration change.
   */
  UpdateSubtreeOverflow = 1 << 12,

  /**
   * The frame's overflow area has changed, through a change in its transform.
   * In other words, the frame's pre-transform overflow is unchanged, but
   * its post-transform overflow has changed, and thus its effect on its
   * parent's overflow has changed.  If the pre-transform overflow has
   * changed, see UpdateOverflow.
   * Does not update any descendant frames.
   */
  UpdatePostTransformOverflow = 1 << 13,

  /**
   * This frame's effect on its parent's overflow area has changed.
   * (But neither its pre-transform nor post-transform overflow have
   * changed; if those are the case, see
   * UpdatePostTransformOverflow.)
   */
  UpdateParentOverflow = 1 << 14,

  /**
   * The children-only transform of an SVG frame changed, requiring overflows to
   * be updated.
   */
  ChildrenOnlyTransform = 1 << 15,

  /**
   * The frame's offsets have changed, while its dimensions might have
   * changed as well.  This hint is used for positioned frames if their
   * offset changes.  If we decide that the dimensions are likely to
   * change, this will trigger a reflow.
   *
   * Note that this should probably be used in combination with
   * UpdateOverflow in order to get the overflow areas of
   * the ancestors updated as well.
   */
  RecomputePosition = 1 << 16,

  /**
   * Behaves like ReconstructFrame, but only if the frame has descendants
   * that are absolutely or fixed position. Use this hint when a style change
   * has changed whether the frame is a container for fixed-pos or abs-pos
   * elements, but reframing is otherwise not needed.
   *
   * Note that ComputedStyle::CalcStyleDifference adjusts results
   * returned by style struct CalcDifference methods to return this hint
   * only if there was a change to whether the element's overall style
   * indicates that it establishes a containing block.
   */
  UpdateContainingBlock = 1 << 17,

  /**
   * This change hint has *no* change handling behavior.  However, it
   * exists to be a non-inherited hint, because when the border-style
   * changes, and it's inherited by a child, that might require a reflow
   * due to the border-width change on the child.
   */
  BorderStyleNoneChange = 1 << 18,

  /**
   * SVG textPath needs to be recomputed because the path has changed.
   * This means that the glyph positions of the text need to be recomputed.
   */
  UpdateTextPath = 1 << 19,

  /**
   * This will schedule an invalidating paint. This is useful if something
   * has changed which will be invalidated by DLBI.
   */
  SchedulePaint = 1 << 20,

  /**
   * A hint reflecting that style data changed with no change handling
   * behavior.  We need to return this, rather than nsChangeHint(0),
   * so that certain optimizations that manipulate the style tree are
   * correct.
   *
   * NeutralChange must be returned by CalcDifference on a given
   * style struct if the data in the style structs are meaningfully different
   * and if no other change hints are returned.  If any other change hints are
   * set, then NeutralChange need not also be included, but it is
   * safe to do so.  (An example of style structs having non-meaningfully
   * different data would be cached information that would be re-calculated
   * to the same values, such as nsStyleBorder::mSubImages.)
   */
  NeutralChange = 1 << 21,

  /**
   * This will cause rendering observers to be invalidated.
   */
  InvalidateRenderingObservers = 1 << 22,

  /**
   * Indicates that the reflow changes the size or position of the
   * element, and thus the reflow must start from at least the frame's
   * parent.  Must be not be set without also setting NeedReflow.
   * And consider adding ClearAncestorIntrinsics if needed.
   */
  ReflowChangesSizeOrPosition = 1 << 23,

  /**
   * Indicates that the style changes the computed BSize --- e.g. 'height'.
   * Must not be set without also setting NeedReflow.
   */
  UpdateComputedBSize = 1 << 24,

  /**
   * Indicates that the 'opacity' property changed between 1 and non-1.
   *
   * Used as extra data for handling UpdateOpacityLayer hints.
   *
   * Note that we do not send this hint if the non-1 value was 0.99 or
   * greater, since in that case we send a RepaintFrame hint instead.
   */
  UpdateUsesOpacity = 1 << 25,

  /**
   * Indicates that the 'background-position' property changed.
   * Regular frames can invalidate these changes using DLBI, but
   * for some frame types we need to repaint the whole frame because
   * the frame does not build individual background image display items
   * for each background layer.
   */
  UpdateBackgroundPosition = 1 << 26,

  /**
   * Indicates that a frame has changed to or from having the CSS
   * transform property set.
   */
  AddOrRemoveTransform = 1 << 27,

  /**
   * Indicates that the presence of scrollbars might have changed.
   *
   * This happens when at least one of overflow-{x,y} properties changed.
   *
   * In most cases, this is equivalent to ReconstructFrame. But
   * in some special cases where the change is really targeting the viewport's
   * scrollframe, this is instead equivalent to nsChangeHint_AllReflowHints
   * (because the viewport always has an associated scrollframe).
   */
  ScrollbarChange = 1 << 28,

  /**
   * Indicates that nsIFrame::UpdateWidgetProperties needs to be called.
   * This is used for -moz-window-* properties.
   */
  UpdateWidgetProperties = 1 << 29,

  /**
   *  Indicates that there has been a colspan or rowspan attribute change
   *  on the cells of a table.
   */
  UpdateTableCellSpans = 1 << 30,

  /**
   * Indicates that the visiblity property changed.
   * This change hint is used for skip restyling for animations on
   * visibility:hidden elements in the case where the elements have no visible
   * descendants.
   */
  VisibilityChange = 1u << 31,

  // IMPORTANT NOTE: When adding a new hint, you will need to add it to
  // one of:
  //
  //   * nsChangeHint_Hints_NeverHandledForDescendants
  //   * nsChangeHint_Hints_AlwaysHandledForDescendants
  //   * nsChangeHint_Hints_SometimesHandledForDescendants
  //
  // and you also may need to handle it in NS_HintsNotHandledForDescendantsIn.
  //
  // Please also add it to RestyleManager::ChangeHintToString and
  // modify All_Hints below accordingly.

  /**
   * Dummy hint value for all hints. It exists for compile time check.
   */
  All_Hints = uint32_t((1ull << 32) - 1),
};

// Redefine these operators to return nothing. This will catch any use
// of these operators on hints. We should not be using these operators
// on nsChangeHints
// FIXME: Fix for CastableTypedEnumResult<nsChangeHint> too!
// FIXME: Only != and == needed now??
inline void operator<(nsChangeHint s1, nsChangeHint s2) {}
inline void operator>(nsChangeHint s1, nsChangeHint s2) {}
inline void operator!=(nsChangeHint s1, nsChangeHint s2) {}
inline void operator==(nsChangeHint s1, nsChangeHint s2) {}
inline void operator<=(nsChangeHint s1, nsChangeHint s2) {}
inline void operator>=(nsChangeHint s1, nsChangeHint s2) {}

// Operators on nsChangeHints

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(nsChangeHint)

// Returns true iff the second hint contains all the hints of the first hint
inline bool NS_IsHintSubset(nsChangeHint aSubset, nsChangeHint aSuperSet) {
  // cast to avoid the operator== above
  typedef mozilla::detail::UnsignedIntegerTypeForEnum<nsChangeHint>::Type
      nsChangeHint_int_type;
  return nsChangeHint_int_type(aSubset & aSuperSet) ==
      nsChangeHint_int_type(aSubset);
}

/**
 * We have an optimization when processing change hints which prevents
 * us from visiting the descendants of a node when a hint on that node
 * is being processed.  This optimization does not apply in some of the
 * cases where applying a hint to an element does not necessarily result
 * in the same hint being handled on the descendants.
 */

// The change hints that are always handled for descendants.
#define nsChangeHint_Hints_AlwaysHandledForDescendants                     \
  (nsChangeHint::ClearDescendantIntrinsics | nsChangeHint::NeedDirtyReflow | \
   nsChangeHint::NeutralChange | nsChangeHint::ReconstructFrame |            \
   nsChangeHint::RepaintFrame | nsChangeHint::SchedulePaint |                \
   nsChangeHint::SyncFrameView | nsChangeHint::UpdateCursor |                \
   nsChangeHint::UpdateSubtreeOverflow | nsChangeHint::UpdateTextPath |      \
   nsChangeHint::VisibilityChange)

// The change hints that are never handled for descendants.
#define nsChangeHint_Hints_NeverHandledForDescendants                         \
  (nsChangeHint::BorderStyleNoneChange | nsChangeHint::ChildrenOnlyTransform |  \
   nsChangeHint::ScrollbarChange | nsChangeHint::InvalidateRenderingObservers | \
   nsChangeHint::RecomputePosition | nsChangeHint::UpdateBackgroundPosition |   \
   nsChangeHint::UpdateComputedBSize | nsChangeHint::UpdateContainingBlock |    \
   nsChangeHint::UpdateEffects | nsChangeHint::UpdateOpacityLayer |             \
   nsChangeHint::UpdateOverflow | nsChangeHint::UpdateParentOverflow |          \
   nsChangeHint::UpdatePostTransformOverflow |                                 \
   nsChangeHint::UpdateTableCellSpans | nsChangeHint::UpdateTransformLayer |    \
   nsChangeHint::UpdateUsesOpacity | nsChangeHint::AddOrRemoveTransform |       \
   nsChangeHint::UpdateWidgetProperties)

// The change hints that are sometimes considered to be handled for descendants.
#define nsChangeHint_Hints_SometimesHandledForDescendants           \
  (nsChangeHint::ClearAncestorIntrinsics | nsChangeHint::NeedReflow | \
   nsChangeHint::ReflowChangesSizeOrPosition)

static_assert(!(nsChangeHint_Hints_AlwaysHandledForDescendants &
                nsChangeHint_Hints_NeverHandledForDescendants) &&
                  !(nsChangeHint_Hints_AlwaysHandledForDescendants &
                    nsChangeHint_Hints_SometimesHandledForDescendants) &&
                  !(nsChangeHint_Hints_NeverHandledForDescendants &
                    nsChangeHint_Hints_SometimesHandledForDescendants) &&
                  !(nsChangeHint::All_Hints ^
                    nsChangeHint_Hints_AlwaysHandledForDescendants ^
                    nsChangeHint_Hints_NeverHandledForDescendants ^
                    nsChangeHint_Hints_SometimesHandledForDescendants),
              "change hints must be present in exactly one of "
              "nsChangeHint_Hints_{Always,Never,Sometimes}"
              "HandledForDescendants");

// The most hints that NS_HintsNotHandledForDescendantsIn could possibly return:
#define nsChangeHint_Hints_NotHandledForDescendants \
  (nsChangeHint_Hints_NeverHandledForDescendants |  \
   nsChangeHint_Hints_SometimesHandledForDescendants)

// Redefine the old NS_STYLE_HINT constants in terms of the new hint structure
#define NS_STYLE_HINT_VISUAL                                            \
  nsChangeHint(nsChangeHint::RepaintFrame | nsChangeHint::SyncFrameView | \
               nsChangeHint::SchedulePaint)
#define nsChangeHint_AllReflowHints                                        \
  nsChangeHint(                                                            \
      nsChangeHint::NeedReflow | nsChangeHint::ReflowChangesSizeOrPosition | \
      nsChangeHint::ClearAncestorIntrinsics |                               \
      nsChangeHint::ClearDescendantIntrinsics | nsChangeHint::NeedDirtyReflow)

// Below are the change hints that we send for ISize & BSize changes.
// Each is similar to nsChangeHint_AllReflowHints with a few changes.

// * For an ISize change, we send nsChangeHint_AllReflowHints, with two bits
// excluded: nsChangeHint::ClearDescendantIntrinsics (because an ancestor's
// inline-size change can't affect descendant intrinsic sizes), and
// nsChangeHint::NeedDirtyReflow (because ISize changes don't need to *force*
// all descendants to reflow).
#define nsChangeHint_ReflowHintsForISizeChange            \
  nsChangeHint(nsChangeHint_AllReflowHints &              \
               ~(nsChangeHint::ClearDescendantIntrinsics | \
                 nsChangeHint::NeedDirtyReflow))

// * For a BSize change, we send almost the same hints as for ISize changes,
// with one extra: nsChangeHint::UpdateComputedBSize.  We need this hint because
// BSize changes CAN affect descendant intrinsic sizes, due to replaced
// elements with percentage BSizes in descendants which also have percentage
// BSizes. nsChangeHint::UpdateComputedBSize clears intrinsic sizes for frames
// that have such replaced elements. (We could instead send
// nsChangeHint::ClearDescendantIntrinsics, but that's broader than we need.)
//
// NOTE: You might think that BSize changes could exclude
// nsChangeHint::ClearAncestorIntrinsics (which is inline-axis specific), but we
// do need to send it, to clear cached results from CSS Flex measuring reflows.
#define nsChangeHint_ReflowHintsForBSizeChange                           \
  nsChangeHint(                                                          \
      (nsChangeHint_AllReflowHints | nsChangeHint::UpdateComputedBSize) & \
      ~(nsChangeHint::ClearDescendantIntrinsics |                         \
        nsChangeHint::NeedDirtyReflow))

// * For changes to the float area of an already-floated element, we need all
// reflow hints, but not the ones that apply to descendants.
// Our descendants aren't impacted when our float area only changes
// placement but not size/shape. (e.g. if we change which side we float to).
// But our ancestors/siblings are potentially impacted, so we need to send
// the non-descendant reflow hints.
#define nsChangeHint_ReflowHintsForFloatAreaChange        \
  nsChangeHint(nsChangeHint_AllReflowHints &              \
               ~(nsChangeHint::ClearDescendantIntrinsics | \
                 nsChangeHint::NeedDirtyReflow))

#define NS_STYLE_HINT_REFLOW \
  nsChangeHint(NS_STYLE_HINT_VISUAL | nsChangeHint_AllReflowHints)

#define nsChangeHint_Hints_CanIgnoreIfNotVisible                            \
  nsChangeHint(                                                             \
      NS_STYLE_HINT_VISUAL | nsChangeHint::NeutralChange |                   \
      nsChangeHint::UpdateOpacityLayer | nsChangeHint::AddOrRemoveTransform | \
      nsChangeHint::UpdateContainingBlock | nsChangeHint::UpdateOverflow |    \
      nsChangeHint::UpdatePostTransformOverflow |                            \
      nsChangeHint::UpdateTransformLayer | nsChangeHint::UpdateUsesOpacity |  \
      nsChangeHint::VisibilityChange)

// Change hints for added or removed transform style.
//
// If we've added or removed the transform property, we need to reconstruct the
// frame to add or remove the view object, and also to handle abs-pos and
// fixed-pos containers.
//
// We do not need to apply nsChangeHint::UpdateTransformLayer since
// nsChangeHint::RepaintFrame will forcibly invalidate the frame area and
// ensure layers are rebuilt (or removed).
#define nsChangeHint_ComprehensiveAddOrRemoveTransform \
  nsChangeHint(nsChangeHint::UpdateContainingBlock |    \
               nsChangeHint::AddOrRemoveTransform |     \
               nsChangeHint::UpdateOverflow | nsChangeHint::RepaintFrame)

// NB: Once we drop support for the old style system, this logic should be
// inlined in the Servo style system to eliminate the FFI call.
inline nsChangeHint NS_HintsNotHandledForDescendantsIn(
    nsChangeHint aChangeHint) {
  nsChangeHint result =
      aChangeHint & nsChangeHint_Hints_NeverHandledForDescendants;

  if (!NS_IsHintSubset(nsChangeHint::NeedDirtyReflow, aChangeHint)) {
    if (NS_IsHintSubset(nsChangeHint::NeedReflow, aChangeHint)) {
      // If NeedDirtyReflow is *not* set, then NeedReflow is a
      // non-inherited hint.
      result |= nsChangeHint::NeedReflow;
    }

    if (NS_IsHintSubset(nsChangeHint::ReflowChangesSizeOrPosition,
                        aChangeHint)) {
      // If NeedDirtyReflow is *not* set, then ReflowChangesSizeOrPosition is a
      // non-inherited hint.
      result |= nsChangeHint::ReflowChangesSizeOrPosition;
    }
  }

  if (!NS_IsHintSubset(nsChangeHint::ClearDescendantIntrinsics, aChangeHint) &&
      NS_IsHintSubset(nsChangeHint::ClearAncestorIntrinsics, aChangeHint)) {
    // If ClearDescendantIntrinsics is *not* set, then
    // ClearAncestorIntrinsics is a non-inherited hint.
    result |= nsChangeHint::ClearAncestorIntrinsics;
  }

  MOZ_ASSERT(
      NS_IsHintSubset(result, nsChangeHint_Hints_NotHandledForDescendants),
      "something is inconsistent");

  return result;
}

inline nsChangeHint NS_HintsHandledForDescendantsIn(nsChangeHint aChangeHint) {
  return aChangeHint & ~NS_HintsNotHandledForDescendantsIn(aChangeHint);
}

// Returns the change hints in aOurChange that are not subsumed by those
// in aHintsHandled (which are hints that have been handled by an ancestor).
inline nsChangeHint NS_RemoveSubsumedHints(nsChangeHint aOurChange,
                                           nsChangeHint aHintsHandled) {
  nsChangeHint result =
      aOurChange & ~NS_HintsHandledForDescendantsIn(aHintsHandled);

  if (result &
      (nsChangeHint::ClearAncestorIntrinsics |
       nsChangeHint::ClearDescendantIntrinsics | nsChangeHint::NeedDirtyReflow |
       nsChangeHint::ReflowChangesSizeOrPosition |
       nsChangeHint::UpdateComputedBSize)) {
    result |= nsChangeHint::NeedReflow;
  }

  if (result & (nsChangeHint::ClearDescendantIntrinsics)) {
    MOZ_ASSERT(result & nsChangeHint::ClearAncestorIntrinsics);
    result |=  // nsChangeHint::ClearAncestorIntrinsics |
        nsChangeHint::NeedDirtyReflow;
  }

  return result;
}

namespace mozilla {

using RestyleHint = StyleRestyleHint;

inline RestyleHint RestyleHint::RestyleSubtree() {
  return StyleRestyleHint_RESTYLE_SELF | StyleRestyleHint_RESTYLE_DESCENDANTS;
}

inline RestyleHint RestyleHint::RecascadeSubtree() {
  return StyleRestyleHint_RECASCADE_SELF |
         StyleRestyleHint_RECASCADE_DESCENDANTS;
}

inline RestyleHint RestyleHint::ForAnimations() {
  return StyleRestyleHint_RESTYLE_CSS_TRANSITIONS |
         StyleRestyleHint_RESTYLE_CSS_ANIMATIONS |
         StyleRestyleHint_RESTYLE_SMIL;
}

}  // namespace mozilla

#endif /* nsChangeHint_h___ */
