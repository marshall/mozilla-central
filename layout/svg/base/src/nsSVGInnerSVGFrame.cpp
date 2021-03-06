/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Main header first:
#include "nsSVGInnerSVGFrame.h"

// Keep others in (case-insensitive) order:
#include "gfxContext.h"
#include "nsIFrame.h"
#include "nsISVGChildFrame.h"
#include "nsRenderingContext.h"
#include "nsSVGContainerFrame.h"
#include "nsSVGSVGElement.h"

nsIFrame*
NS_NewSVGInnerSVGFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGInnerSVGFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGInnerSVGFrame)

//----------------------------------------------------------------------
// nsIFrame methods

NS_QUERYFRAME_HEAD(nsSVGInnerSVGFrame)
  NS_QUERYFRAME_ENTRY(nsSVGInnerSVGFrame)
  NS_QUERYFRAME_ENTRY(nsISVGSVGFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsSVGInnerSVGFrameBase)

#ifdef DEBUG
NS_IMETHODIMP
nsSVGInnerSVGFrame::Init(nsIContent* aContent,
                         nsIFrame* aParent,
                         nsIFrame* aPrevInFlow)
{
  nsCOMPtr<nsIDOMSVGSVGElement> svg = do_QueryInterface(aContent);
  NS_ASSERTION(svg, "Content is not an SVG 'svg' element!");

  return nsSVGInnerSVGFrameBase::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

nsIAtom *
nsSVGInnerSVGFrame::GetType() const
{
  return nsGkAtoms::svgInnerSVGFrame;
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods

NS_IMETHODIMP
nsSVGInnerSVGFrame::PaintSVG(nsRenderingContext *aContext,
                             const nsIntRect *aDirtyRect)
{
  gfxContextAutoSaveRestore autoSR;

  if (GetStyleDisplay()->IsScrollableOverflow()) {
    float x, y, width, height;
    static_cast<nsSVGSVGElement*>(mContent)->
      GetAnimatedLengthValues(&x, &y, &width, &height, nsnull);

    if (width <= 0 || height <= 0) {
      return NS_OK;
    }

    nsSVGContainerFrame *parent = static_cast<nsSVGContainerFrame*>(mParent);
    gfxMatrix clipTransform = parent->GetCanvasTM();

    gfxContext *gfx = aContext->ThebesContext();
    autoSR.SetContext(gfx);
    gfxRect clipRect =
      nsSVGUtils::GetClipRectForFrame(this, x, y, width, height);
    nsSVGUtils::SetClipRect(gfx, clipTransform, clipRect);
  }

  return nsSVGInnerSVGFrameBase::PaintSVG(aContext, aDirtyRect);
}

void
nsSVGInnerSVGFrame::NotifySVGChanged(PRUint32 aFlags)
{
  NS_ABORT_IF_FALSE(!(aFlags & DO_NOT_NOTIFY_RENDERING_OBSERVERS) ||
                    (GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD),
                    "Must be NS_STATE_SVG_NONDISPLAY_CHILD!");

  NS_ABORT_IF_FALSE(aFlags & (TRANSFORM_CHANGED | COORD_CONTEXT_CHANGED),
                    "Invalidation logic may need adjusting");

  if (aFlags & COORD_CONTEXT_CHANGED) {

    nsSVGSVGElement *svg = static_cast<nsSVGSVGElement*>(mContent);

    // Coordinate context changes affect mCanvasTM if we have a
    // percentage 'x' or 'y', or if we have a percentage 'width' or 'height' AND
    // a 'viewBox'.

    if (!(aFlags & TRANSFORM_CHANGED) &&
        (svg->mLengthAttributes[nsSVGSVGElement::X].IsPercentage() ||
         svg->mLengthAttributes[nsSVGSVGElement::Y].IsPercentage() ||
         (svg->HasViewBox() &&
          (svg->mLengthAttributes[nsSVGSVGElement::WIDTH].IsPercentage() ||
           svg->mLengthAttributes[nsSVGSVGElement::HEIGHT].IsPercentage())))) {
    
      aFlags |= TRANSFORM_CHANGED;
    }

    // XXX We could clear the COORD_CONTEXT_CHANGED flag in some circumstances
    // if we have a non-percentage 'width' AND 'height, or if we have a 'viewBox'
    // rect. This is because, when we have a viewBox rect, the viewBox rect
    // is the coordinate context for our children, and it isn't changing.
    // Percentage lengths on our children will continue to resolve to the
    // same number of user units because they're relative to our viewBox rect. The
    // same is true if we have a non-percentage width and height and don't have a
    // viewBox. We (the <svg>) establish the coordinate context for our children. Our
    // children don't care about changes to our parent coordinate context unless that
    // change results in a change to the coordinate context that _we_ establish. Hence
    // we can (should, really) stop propagating COORD_CONTEXT_CHANGED in these cases.
    // We'd actually need to check that we have a viewBox rect and not just
    // that viewBox is set, since it could be set to none.
    // Take care not to break the testcase for bug 394463 when implementing this
  }

  if (aFlags & TRANSFORM_CHANGED) {
    // make sure our cached transform matrix gets (lazily) updated
    mCanvasTM = nsnull;
  }

  nsSVGInnerSVGFrameBase::NotifySVGChanged(aFlags);
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::AttributeChanged(PRInt32  aNameSpaceID,
                                     nsIAtom* aAttribute,
                                     PRInt32  aModType)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::width ||
        aAttribute == nsGkAtoms::height) {

      nsSVGSVGElement* svg = static_cast<nsSVGSVGElement*>(mContent);
      if (svg->HasViewBox()) {

        // make sure our cached transform matrix gets (lazily) updated
        mCanvasTM = nsnull;

        nsSVGUtils::NotifyChildrenOfSVGChange(this, TRANSFORM_CHANGED);
      } else {

        PRUint32 flags = COORD_CONTEXT_CHANGED;

        if (mCanvasTM && mCanvasTM->IsSingular()) {

          mCanvasTM = nsnull;

          flags |= TRANSFORM_CHANGED;
        }
        nsSVGUtils::NotifyChildrenOfSVGChange(this, flags);
      }

    } else if (aAttribute == nsGkAtoms::transform ||
               aAttribute == nsGkAtoms::preserveAspectRatio ||
               aAttribute == nsGkAtoms::viewBox ||
               aAttribute == nsGkAtoms::x ||
               aAttribute == nsGkAtoms::y) {
      // make sure our cached transform matrix gets (lazily) updated
      mCanvasTM = nsnull;

      nsSVGUtils::NotifyChildrenOfSVGChange(
          this, aAttribute == nsGkAtoms::viewBox ?
                  TRANSFORM_CHANGED | COORD_CONTEXT_CHANGED : TRANSFORM_CHANGED);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP_(nsIFrame*)
nsSVGInnerSVGFrame::GetFrameForPoint(const nsPoint &aPoint)
{
  if (GetStyleDisplay()->IsScrollableOverflow()) {
    nsSVGElement *content = static_cast<nsSVGElement*>(mContent);
    nsSVGContainerFrame *parent = static_cast<nsSVGContainerFrame*>(mParent);

    float clipX, clipY, clipWidth, clipHeight;
    content->GetAnimatedLengthValues(&clipX, &clipY, &clipWidth, &clipHeight, nsnull);

    if (!nsSVGUtils::HitTestRect(parent->GetCanvasTM(),
                                 clipX, clipY, clipWidth, clipHeight,
                                 PresContext()->AppUnitsToDevPixels(aPoint.x),
                                 PresContext()->AppUnitsToDevPixels(aPoint.y))) {
      return nsnull;
    }
  }

  return nsSVGInnerSVGFrameBase::GetFrameForPoint(aPoint);
}

//----------------------------------------------------------------------
// nsISVGSVGFrame methods:

void
nsSVGInnerSVGFrame::NotifyViewportOrTransformChanged(PRUint32 aFlags)
{
  // The dimensions of inner-<svg> frames are purely defined by their "width"
  // and "height" attributes, and transform changes can only occur as a result
  // of changes to their "width", "height", "viewBox" or "preserveAspectRatio"
  // attributes. Changes to all of these attributes are handled in
  // AttributeChanged(), so we should never be called.
  NS_ERROR("Not called for nsSVGInnerSVGFrame");
}

//----------------------------------------------------------------------
// nsSVGContainerFrame methods:

gfxMatrix
nsSVGInnerSVGFrame::GetCanvasTM()
{
  if (!mCanvasTM) {
    NS_ASSERTION(mParent, "null parent");

    nsSVGContainerFrame *parent = static_cast<nsSVGContainerFrame*>(mParent);
    nsSVGSVGElement *content = static_cast<nsSVGSVGElement*>(mContent);

    gfxMatrix tm = content->PrependLocalTransformsTo(parent->GetCanvasTM());

    mCanvasTM = new gfxMatrix(tm);
  }
  return *mCanvasTM;
}

