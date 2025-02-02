/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGGENERICCONTAINERFRAME_H__
#define __NS_SVGGENERICCONTAINERFRAME_H__

#include "mozilla/Attributes.h"
#include "gfxMatrix.h"
#include "nsIFrame.h"
#include "nsLiteralString.h"
#include "nsQueryFrame.h"
#include "nsSVGContainerFrame.h"

class nsAtom;
class nsIFrame;

namespace mozilla {
class PresShell;
}  // namespace mozilla

nsIFrame* NS_NewSVGGenericContainerFrame(mozilla::PresShell* aPresShell,
                                         mozilla::ComputedStyle* aStyle);

namespace mozilla {

class SVGGenericContainerFrame final : public nsSVGDisplayContainerFrame {
  friend nsIFrame* ::NS_NewSVGGenericContainerFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle);

 protected:
  explicit SVGGenericContainerFrame(ComputedStyle* aStyle,
                                    nsPresContext* aPresContext)
      : nsSVGDisplayContainerFrame(aStyle, aPresContext, kClassID) {}

 public:
  NS_DECL_FRAMEARENA_HELPERS(SVGGenericContainerFrame)

  // nsIFrame:
  virtual nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                    int32_t aModType) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SVGGenericContainer"_ns, aResult);
  }
#endif

  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM() override;
};

}  // namespace mozilla

#endif  // __NS_SVGGENERICCONTAINERFRAME_H__
