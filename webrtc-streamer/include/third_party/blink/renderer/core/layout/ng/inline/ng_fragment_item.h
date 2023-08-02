// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_FRAGMENT_ITEM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_FRAGMENT_ITEM_H_

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_text_offset.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_text_type.h"
#include "third_party/blink/renderer/core/layout/ng/ng_ink_overflow.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_client.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class NGFragmentItems;
class NGInlineBreakToken;
struct NGTextFragmentPaintInfo;
struct NGLogicalLineItem;

// Data for SVG text in addition to NGFragmentItem.
struct NGSvgFragmentData {
  USING_FAST_MALLOC(NGSvgFragmentData);

 public:
  scoped_refptr<const ShapeResultView> shape_result;
  NGTextOffset text_offset;
  gfx::RectF rect;
  float length_adjust_scale;
  float angle;
  float baseline_shift;
  bool in_text_path;
};

// This class represents a text run or a box in an inline formatting context.
//
// This class consumes less memory than a full fragment, and can be stored in a
// flat list (NGFragmentItems) for easier and faster traversal.
class CORE_EXPORT NGFragmentItem final {
  DISALLOW_NEW();

 public:
  // Represents regular text that exists in the DOM.
  struct TextItem {
    DISALLOW_NEW();
    scoped_refptr<const ShapeResultView> shape_result;
    // TODO(kojii): |text_offset| should match to the offset in |shape_result|.
    // Consider if we should remove them, or if keeping them is easier.
    const NGTextOffset text_offset;
  };
  // Represents text in SVG <text>.
  struct SvgTextItem {
    DISALLOW_NEW();
    std::unique_ptr<NGSvgFragmentData> data;
  };
  // Represents text generated by the layout engine, e.g., hyphen or ellipsis.
  struct GeneratedTextItem {
    DISALLOW_NEW();
    scoped_refptr<const ShapeResultView> shape_result;
    String text;
  };
  // A start marker of a line box.
  struct LineItem {
    DISALLOW_NEW();

   public:
    void Trace(Visitor* visitor) const { visitor->Trace(line_box_fragment); }
    Member<const NGPhysicalLineBoxFragment> line_box_fragment;
    wtf_size_t descendants_count;
  };
  // Represents a box fragment appeared in a line. This includes inline boxes
  // (e.g., <span>text</span>) and atomic inlines.
  struct BoxItem {
    DISALLOW_NEW();
    // This copy constructor looks up the "post-layout" fragment.
    BoxItem(const BoxItem&);
    BoxItem(const NGPhysicalBoxFragment*, wtf_size_t descendants_count);

    // If this item is an inline box, its children are stored as following
    // items. |descendants_count_| has the number of such items.
    //
    // If this item is a root of another IFC/BFC, children are stored normally,
    // as children of |box_fragment|.
    const NGPhysicalBoxFragment* PostLayout() const;

    void Trace(Visitor*) const;

    Member<const NGPhysicalBoxFragment> box_fragment;
    wtf_size_t descendants_count;
  };

  enum ItemType { kText, kSvgText, kGeneratedText, kLine, kBox };
  enum TracedType { kNone, kLineItem, kBoxItem };

  // Create appropriate type for |line_item|.
  NGFragmentItem(NGLogicalLineItem&& line_item, WritingMode writing_mode);
  // Create a box item.
  NGFragmentItem(const NGPhysicalBoxFragment& box,
                 TextDirection resolved_direction);
  // Create a line item.
  explicit NGFragmentItem(const NGPhysicalLineBoxFragment& line);

  // The copy/move constructors.
  NGFragmentItem(const NGFragmentItem&);
  NGFragmentItem(NGFragmentItem&&);

  ~NGFragmentItem();

  ItemType Type() const { return static_cast<ItemType>(type_); }

  bool IsText() const {
    return Type() == kText || Type() == kSvgText || Type() == kGeneratedText;
  }
  bool IsContainer() const { return Type() == kBox || Type() == kLine; }
  bool IsInlineBox() const;
  bool IsAtomicInline() const;
  bool IsBlockInInline() const;
  bool IsFloating() const;
  bool IsEmptyLineBox() const;
  bool IsHiddenForPaint() const { return is_hidden_for_paint_; }
  bool IsListMarker() const;

  // Make this kSvgText type. |this| type must be kText.
  void ConvertToSvgText(std::unique_ptr<NGSvgFragmentData> data,
                        const PhysicalRect& unscaled_rect,
                        bool is_hidden);
  void SetSvgLineLocalRect(const PhysicalRect& unscaled_rect);

  // A sequence number of fragments generated from a |LayoutObject|.
  // For line boxes, this is a sequence number for the containing
  // |LayoutBlockFlow|, starting at |kInitialLineFragmentId|.
  wtf_size_t FragmentId() const {
    DCHECK(Type() != kLine || fragment_id_ >= kInitialLineFragmentId);
    return fragment_id_;
  }
  void SetFragmentId(wtf_size_t id) const {
    DCHECK(Type() != kLine || id >= kInitialLineFragmentId);
    fragment_id_ = id;
  }
  // The initial framgent_id for line boxes.
  // TODO(kojii): This is to avoid conflict with multicol because line boxes use
  // its |LayoutBlockFlow| as their |DisplayItemClient|, but multicol also uses
  // fragment id for |LayoutBlockFlow| today. The plan is to make |FragmentData|
  // a |DisplayItemClient| instead.
  static constexpr wtf_size_t kInitialLineFragmentId = 0x80000000;

  // Return true if this is the first fragment generated from a node.
  bool IsFirstForNode() const { return !FragmentId(); }

  // Return true if this is the last fragment generated from a node.
  bool IsLastForNode() const {
    DCHECK(Type() != kLine);
    return is_last_for_node_;
  }
  void SetIsLastForNode(bool is_last) const { is_last_for_node_ = is_last; }

  NGStyleVariant StyleVariant() const {
    return static_cast<NGStyleVariant>(style_variant_);
  }
  bool UsesFirstLineStyle() const {
    return StyleVariant() == NGStyleVariant::kFirstLine;
  }
  // Returns the style for this fragment.
  //
  // For a line box, this returns the style of the containing block. This mostly
  // represents the style for the line box, except 1) |style.Direction()| maybe
  // incorrect, use |BaseDirection()| instead, and 2) margin/border/padding,
  // background etc. do not apply to the line box.
  const ComputedStyle& Style() const {
    return layout_object_->EffectiveStyle(StyleVariant());
  }
  const LayoutObject* GetLayoutObject() const { return layout_object_; }
  LayoutObject* GetMutableLayoutObject() const {
    return const_cast<LayoutObject*>(layout_object_.Get());
  }
  bool IsLayoutObjectDestroyedOrMoved() const { return !layout_object_; }
  void LayoutObjectWillBeDestroyed() const;
  void LayoutObjectWillBeMoved() const;
  Node* GetNode() const { return layout_object_->GetNode(); }
  Node* NodeForHitTest() const { return layout_object_->NodeForHitTest(); }

  // Use |LayoutObject|+|FragmentId()| for |DisplayItem::Id|.
  const DisplayItemClient* GetDisplayItemClient() const {
    return GetLayoutObject();
  }

  bool IsRelayoutBoundary() const {
    return layout_object_->IsRelayoutBoundary();
  }

  wtf_size_t DeltaToNextForSameLayoutObject() const {
    return delta_to_next_for_same_layout_object_;
  }
  void SetDeltaToNextForSameLayoutObject(wtf_size_t delta) const;

  const PhysicalRect& RectInContainerFragment() const { return rect_; }
  // This function returns a transformed unscaled glyph bounds for kSvgText
  // type.
  // Do not call this for other types.
  gfx::RectF ObjectBoundingBox(const NGFragmentItems& items) const;

  // Returns a point transformed by the inverse of
  // BuildSvgTransformForBoundingBox(). The return value can be compared with
  // untransformed RectInContainerFragment().
  PhysicalOffset MapPointInContainer(const PhysicalOffset& point) const;

  // For kSvgText type, convert the specified inline offset in this item so
  // that the result can be used with ShapeResult.
  float ScaleInlineOffset(LayoutUnit inline_offset) const;

  // Returns true if |position|, which is a point in the IFC's coordinate
  // system, is in the transformed rectangle (including the edges) of this item.
  // This works only for kSvgText type.
  bool InclusiveContains(const gfx::PointF& position) const;

  const PhysicalOffset& OffsetInContainerFragment() const {
    return rect_.offset;
  }
  const PhysicalSize& Size() const { return rect_.size; }
  PhysicalRect LocalRect() const { return {PhysicalOffset(), Size()}; }
  void SetOffset(const PhysicalOffset& offset) { rect_.offset = offset; }

  PhysicalRect InkOverflow() const;
  PhysicalRect SelfInkOverflow() const;

  // Count of following items that are descendants of this item in the box tree,
  // including this item. 1 means this is a box (box or line box) without
  // descendants. 0 if this item type cannot have children.
  wtf_size_t DescendantsCount() const {
    if (Type() == kBox)
      return box_.descendants_count;
    if (Type() == kLine)
      return line_.descendants_count;
    DCHECK(!IsContainer());
    return 0;
  }
  bool HasChildren() const { return DescendantsCount() > 1; }
  void SetDescendantsCount(wtf_size_t count) {
    if (Type() == kBox) {
      box_.descendants_count = count;
      return;
    }
    if (Type() == kLine) {
      line_.descendants_count = count;
      return;
    }
    NOTREACHED();
  }

  // Returns |NGPhysicalBoxFragment| if one is associated with this item.
  const NGPhysicalBoxFragment* BoxFragment() const {
    if (Type() == kBox)
      return box_.box_fragment;
    return nullptr;
  }
  const NGPhysicalBoxFragment* PostLayoutBoxFragment() const {
    if (Type() == kBox)
      return box_.PostLayout();
    return nullptr;
  }

  // Returns block of block-in-inline. Note: We can have LayoutBlock and
  // LayoutImage. See http://crbug.com/1295087
  LayoutObject& BlockInInline() const;

  bool HasNonVisibleOverflow() const;
  bool IsScrollContainer() const;
  bool HasSelfPaintingLayer() const;

  // TODO(kojii): Avoid using this function in outside of this class as much as
  // possible, because |NGPhysicalLineBoxFragment| is likely to be removed. Add
  // functions to access data in |NGPhysicalLineBoxFragment| rather than using
  // this function. See |InlineBreakToken()| for example.
  const NGPhysicalLineBoxFragment* LineBoxFragment() const {
    if (Type() == kLine)
      return line_.line_box_fragment;
    return nullptr;
  }

  // Returns |NGInlineBreakToken| associated with this line, for line items.
  // Calling this function for other types is not valid.
  const NGInlineBreakToken* InlineBreakToken() const {
    if (const NGPhysicalLineBoxFragment* line_box = LineBoxFragment())
      return To<NGInlineBreakToken>(line_box->BreakToken());
    NOTREACHED();
    return nullptr;
  }

  using NGLineBoxType = NGPhysicalLineBoxFragment::NGLineBoxType;
  NGLineBoxType LineBoxType() const {
    if (Type() == kLine)
      return static_cast<NGLineBoxType>(sub_type_);
    NOTREACHED() << this;
    return NGLineBoxType::kNormalLineBox;
  }

  static PhysicalRect LocalVisualRectFor(const LayoutObject& layout_object);

  // Re-compute the ink overflow for the |cursor| until its end.
  static PhysicalRect RecalcInkOverflowForCursor(NGInlineCursor* cursor);

  // Painters can use const methods only, except for these explicitly declared
  // methods.
  class MutableForPainting {
    STACK_ALLOCATED();

   public:
    void InvalidateInkOverflow() { return item_.InvalidateInkOverflow(); }
    void RecalcInkOverflow(const NGInlineCursor& cursor,
                           PhysicalRect* self_and_contents_rect_out) {
      return item_.RecalcInkOverflow(cursor, self_and_contents_rect_out);
    }

   private:
    friend class NGFragmentItem;
    explicit MutableForPainting(const NGFragmentItem& item)
        : item_(const_cast<NGFragmentItem&>(item)) {}

    NGFragmentItem& item_;
  };

  MutableForPainting GetMutableForPainting() const {
    return MutableForPainting(*this);
  }

  // Out-of-flow in nested block fragmentation may require us to replace a
  // fragment on a line.
  class MutableForOOFFragmentation {
    STACK_ALLOCATED();

   public:
    void ReplaceBoxFragment(const NGPhysicalBoxFragment& new_fragment) {
      DCHECK(item_.BoxFragment());
      item_.box_.box_fragment = &new_fragment;
    }

   private:
    friend class NGFragmentItem;
    explicit MutableForOOFFragmentation(const NGFragmentItem& item)
        : item_(const_cast<NGFragmentItem&>(item)) {}

    NGFragmentItem& item_;
  };

  MutableForOOFFragmentation GetMutableForOOFFragmentation() const {
    return MutableForOOFFragmentation(*this);
  }

  bool IsHorizontal() const {
    return IsHorizontalWritingMode(GetWritingMode());
  }

  WritingMode GetWritingMode() const {
    return Style().GetWritingMode();
  }

  // Functions for |TextItem|, |SvgTextItem|, and |GeneratedTextItem|.
  NGTextType TextType() const {
    if (Type() == kText || Type() == kSvgText)
      return static_cast<NGTextType>(sub_type_);
    if (Type() == kGeneratedText)
      return NGTextType::kLayoutGenerated;
    NOTREACHED() << this;
    return NGTextType::kNormal;
  }

  // True if this is a forced line break.
  bool IsLineBreak() const {
    return TextType() == NGTextType::kForcedLineBreak;
  }

  // True if this is not for painting; i.e., a forced line break, a tabulation,
  // or a soft-wrap opportunity.
  bool IsFlowControl() const {
    return IsLineBreak() || TextType() == NGTextType::kFlowControl;
  }

  // True if this is an ellpisis generated by `text-overflow: ellipsis`.
  bool IsEllipsis() const {
    return StyleVariant() == NGStyleVariant::kEllipsis;
  }

  // Returns true if the text is generated (from, e.g., list marker,
  // pseudo-element, ...) instead of from a DOM text node.
  //  * CSS content         kText
  //  * ellipsis            kGeneratedText
  //  * first-letter-part   kText
  //  * list marker         kGeneratedText
  //  * hyphen (soft/auto)  kGeneratedText
  bool IsLayoutGeneratedText() const { return Type() == kGeneratedText; }
  bool IsStyleGeneratedText() const;
  bool IsGeneratedText() const;

  bool IsFormattingContextRoot() const;

  bool IsSymbolMarker() const {
    return TextType() == NGTextType::kSymbolMarker;
  }

  const ShapeResultView* TextShapeResult() const;
  NGTextOffset TextOffset() const;
  unsigned StartOffset() const { return TextOffset().start; }
  unsigned EndOffset() const { return TextOffset().end; }
  unsigned TextLength() const { return TextOffset().Length(); }

  // Layout-generated text has two offsets; one for its own generated string,
  // and the other for the container. |TextOffset| returns the former, while
  // this function returns the latter.
  unsigned StartOffsetInContainer(const NGInlineCursor& container) const;

  StringView Text(const NGFragmentItems& items) const;
  String GeneratedText() const {
    DCHECK_EQ(Type(), kGeneratedText);
    return generated_text_.text;
  }
  NGTextFragmentPaintInfo TextPaintInfo(const NGFragmentItems& items) const;

  // Compute the inline position from text offset, in logical coordinate
  // relative to this fragment suitable for |LocalCaretRect|.
  LayoutUnit CaretInlinePositionForOffset(StringView text,
                                          unsigned offset) const;

  // Compute line-relative coordinates for given offsets, this is not
  // flow-relative:
  // This returns scaled values for kSVGText type.
  // https://drafts.csswg.org/css-writing-modes-3/#line-directions
  std::pair<LayoutUnit, LayoutUnit> LineLeftAndRightForOffsets(
      StringView text,
      unsigned start_offset,
      unsigned end_offset) const;

  // The layout box of text in (start, end) range in local coordinate.
  // Start and end offsets must be between StartOffset() and EndOffset().
  // This returns a scaled PhysicalRect for kSVGText type.
  PhysicalRect LocalRect(StringView text,
                         unsigned start_offset,
                         unsigned end_offset) const;

  // The base direction of line. Also known as the paragraph direction. This may
  // be different from the direction of the container box when first-line style
  // is used, or when 'unicode-bidi: plaintext' is used.
  // Note: This is valid only for |LineItem|.
  TextDirection BaseDirection() const;

  // Direction of this item valid for |TextItem| and |IsAtomicInline()|.
  // Note: <span> doesn't have text direction.
  TextDirection ResolvedDirection() const;

  // Returns |PhysicalRect| to intersect with hit test location for |this|
  // text item. See |NGBoxFragmentPainter::HitTestTextItem()|.
  PhysicalRect ComputeTextBoundsRectForHitTest(
      const PhysicalOffset& inline_root_offset,
      bool is_occlusion_test) const;

  // Converts the given point, relative to the fragment itself, into a position
  // in DOM tree.
  PositionWithAffinity PositionForPointInText(
      const PhysicalOffset& point,
      const NGInlineCursor& cursor) const;
  PositionWithAffinity PositionForPointInText(
      unsigned text_offset,
      const NGInlineCursor& cursor) const;
  unsigned TextOffsetForPoint(const PhysicalOffset& point,
                              const NGFragmentItems& items) const;

  // Whether this item was marked dirty for reuse or not.
  bool IsDirty() const { return is_dirty_; }
  void SetDirty() const { is_dirty_ = true; }

  // Returns true if this item is reusable.
  bool CanReuse() const;

  const NGFragmentItem* operator->() const { return this; }

  const NGSvgFragmentData* SvgFragmentData() const {
    return Type() == kSvgText ? svg_text_.data.get() : nullptr;
  }
  // Returns true if BuildSvgTransformForPaint() returns non-identity transform.
  bool HasSvgTransformForPaint() const;
  // A transform which should be used on painting this fragment.
  // This contains a transform for lengthAdjust=spacingAndGlyphs.
  AffineTransform BuildSvgTransformForPaint() const;
  // Returns true if BuildSvgTransformForBoundingBox() returns non-identity
  // transform.
  bool HasSvgTransformForBoundingBox() const;
  // A transform which should be used on computing a bounding box.
  // This contains no transform for lengthAdjust=spacingAndGlyphs because
  // RectInContainerFragment() already takes into account of
  // lengthAdjust=spacingAndGlyphs.
  AffineTransform BuildSvgTransformForBoundingBox() const;

  // Returns a transformed text cell in the unscaled coordination system.
  // This works only with kSvgText type.
  gfx::QuadF SvgUnscaledQuad() const;

  // Returns a font scaling factor for SVG <text>.
  // This returns 1 for an NGFragmentItem not for LayoutSVGInlineText.
  float SvgScalingFactor() const;

  // Return a scaled font for SVG <text>.
  // This returns Style().GetFont() for an NGFragmentItem not for
  // LayoutSVGInlineText.
  const Font& ScaledFont() const;

  // Get a description of |this| for the debug purposes.
  String ToString() const;

  void Trace(Visitor*) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(NGFragmentItemTest, CopyMove);
  FRIEND_TEST_ALL_PREFIXES(NGFragmentItemTest, SelfPaintingInlineBox);
  FRIEND_TEST_ALL_PREFIXES(StyleChangeTest, NeedsCollectInlinesOnStyle);
  friend class LayoutNGTextCombineTest;

  // Create a text item.
  NGFragmentItem(const NGInlineItem& inline_item,
                 scoped_refptr<const ShapeResultView> shape_result,
                 const NGTextOffset& text_offset,
                 const PhysicalSize& size,
                 bool is_hidden_for_paint);
  // Create a generated text item.
  NGFragmentItem(const NGInlineItem& inline_item,
                 scoped_refptr<const ShapeResultView> shape_result,
                 const String& text_content,
                 const PhysicalSize& size,
                 bool is_hidden_for_paint);
  NGFragmentItem(const LayoutObject& layout_object,
                 NGTextType text_type,
                 NGStyleVariant style_variant,
                 TextDirection direction,
                 scoped_refptr<const ShapeResultView> shape_result,
                 const String& text_content,
                 const PhysicalSize& size,
                 bool is_hidden_for_paint);

  NGInkOverflow::Type InkOverflowType() const {
    return static_cast<NGInkOverflow::Type>(ink_overflow_type_);
  }
  bool IsInkOverflowComputed() const {
    return InkOverflowType() != NGInkOverflow::kNotSet &&
           InkOverflowType() != NGInkOverflow::kInvalidated;
  }
  bool HasInkOverflow() const {
    return InkOverflowType() != NGInkOverflow::kNone;
  }
  const LayoutBox* InkOverflowOwnerBox() const;
  LayoutBox* MutableInkOverflowOwnerBox();

  void InvalidateInkOverflow();

  // Re-compute the ink overflow for this item. |cursor| should be at |this|.
  void RecalcInkOverflow(const NGInlineCursor& cursor,
                         PhysicalRect* self_and_contents_rect_out);

  // Compute the inline position from text offset, in logical coordinate
  // relative to this fragment.
  LayoutUnit InlinePositionForOffset(StringView text,
                                     unsigned offset,
                                     LayoutUnit (*round_function)(float),
                                     AdjustMidCluster) const;

  AffineTransform BuildSvgTransformForTextPath(
      const AffineTransform& length_adjust) const;
  AffineTransform BuildSvgTransformForLengthAdjust() const;

  Member<const LayoutObject> layout_object_;

  // TODO(kojii): We can make them sub-classes if we need to make the vector of
  // pointers. Sub-classing from DisplayItemClient prohibits copying and that we
  // cannot create a vector of this class.
  GC_PLUGIN_IGNORE("crbug.com/1146383")
  union {
    TextItem text_;
    SvgTextItem svg_text_;
    GeneratedTextItem generated_text_;
    LineItem line_;
    BoxItem box_;
  };

  PhysicalRect rect_;

  NGInkOverflow ink_overflow_;

  mutable wtf_size_t fragment_id_ = 0;

  // Item index delta to the next item for the same |LayoutObject|.
  mutable wtf_size_t delta_to_next_for_same_layout_object_ = 0;

  // Note: We should not add |bidi_level_| because it is used only for layout.
  const unsigned const_traced_type_ : 2;  // TracedType
  unsigned type_ : 3;                     // ItemType
  unsigned sub_type_ : 3;                 // NGTextType or NGLineBoxType
  unsigned style_variant_ : 2;            // NGStyleVariant
  unsigned is_hidden_for_paint_ : 1;
  // Note: For |TextItem| and |GeneratedTextItem|, |text_direction_| equals to
  // |ShapeResult::Direction()|.
  unsigned text_direction_ : 1;  // TextDirection.

  unsigned ink_overflow_type_ : NGInkOverflow::kTypeBits;

  mutable unsigned is_dirty_ : 1;

  mutable unsigned is_last_for_node_ : 1;
};

inline bool NGFragmentItem::CanReuse() const {
  DCHECK_NE(Type(), kLine);
  if (IsDirty())
    return false;
  if (const LayoutObject* layout_object = GetLayoutObject())
    return !layout_object->SelfNeedsLayout();
  return false;
}

CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGFragmentItem*);
CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGFragmentItem&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_FRAGMENT_ITEM_H_