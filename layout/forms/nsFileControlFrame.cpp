/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFileControlFrame.h"

#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"
#include "nsHTMLParts.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIFormControl.h"
#include "nsINameSpaceManager.h"
#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsPIDOMWindow.h"
#include "nsIFilePicker.h"
#include "nsIDOMMouseEvent.h"
#include "nsINodeInfo.h"
#include "nsIDOMEventTarget.h"
#include "nsIFile.h"
#include "nsHTMLInputElement.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "nsDisplayList.h"
#include "nsEventListenerManager.h"

#include "nsInterfaceHashtable.h"
#include "nsURIHashKey.h"
#include "nsNetCID.h"
#include "nsWeakReference.h"
#include "nsIVariant.h"
#include "mozilla/Services.h"
#include "nsDirectoryServiceDefs.h"
#include "nsICapturePicker.h"
#include "nsIFileURL.h"
#include "nsDOMFile.h"
#include "nsEventStates.h"
#include "nsTextControlFrame.h"

#include "nsIDOMDOMStringList.h"
#include "nsIDOMDragEvent.h"
#include "nsContentList.h"

using namespace mozilla;

#define SYNC_TEXT 0x1
#define SYNC_BUTTON 0x2

nsIFrame*
NS_NewFileControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsFileControlFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsFileControlFrame)

nsFileControlFrame::nsFileControlFrame(nsStyleContext* aContext):
  nsBlockFrame(aContext)
{
  AddStateBits(NS_BLOCK_FLOAT_MGR);
}


NS_IMETHODIMP
nsFileControlFrame::Init(nsIContent* aContent,
                         nsIFrame*   aParent,
                         nsIFrame*   aPrevInFlow)
{
  nsresult rv = nsBlockFrame::Init(aContent, aParent, aPrevInFlow);
  NS_ENSURE_SUCCESS(rv, rv);

  mMouseListener = new BrowseMouseListener(this);
  NS_ENSURE_TRUE(mMouseListener, NS_ERROR_OUT_OF_MEMORY);
  mCaptureMouseListener = new CaptureMouseListener(this);
  NS_ENSURE_TRUE(mCaptureMouseListener, NS_ERROR_OUT_OF_MEMORY);

  return rv;
}

void
nsFileControlFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  ENSURE_TRUE(mContent);

  // Remove the drag events
  if (mContent) {
    mContent->RemoveSystemEventListener(NS_LITERAL_STRING("drop"),
                                        mMouseListener, false);
    mContent->RemoveSystemEventListener(NS_LITERAL_STRING("dragover"),
                                        mMouseListener, false);
  }

  // remove mMouseListener as a mouse event listener (bug 40533, bug 355931)
  nsContentUtils::DestroyAnonymousContent(&mCapture);

  if (mBrowse) {
    mBrowse->RemoveSystemEventListener(NS_LITERAL_STRING("click"),
                                       mMouseListener, false);
  }
  nsContentUtils::DestroyAnonymousContent(&mBrowse);

  if (mTextContent) {
    mTextContent->RemoveSystemEventListener(NS_LITERAL_STRING("click"),
                                            mMouseListener, false);
  }
  nsContentUtils::DestroyAnonymousContent(&mTextContent);

  mCaptureMouseListener->ForgetFrame();
  mMouseListener->ForgetFrame();
  nsBlockFrame::DestroyFrom(aDestructRoot);
}

nsresult
nsFileControlFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  // Get the NodeInfoManager and tag necessary to create input elements
  nsCOMPtr<nsIDocument> doc = mContent->GetDocument();

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nodeInfo = doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::input, nullptr,
                                                 kNameSpaceID_XHTML,
                                                 nsIDOMNode::ELEMENT_NODE);

  // Create the text content
  NS_NewHTMLElement(getter_AddRefs(mTextContent), nodeInfo.forget(),
                    dom::NOT_FROM_PARSER);
  if (!mTextContent)
    return NS_ERROR_OUT_OF_MEMORY;

  // Mark the element to be native anonymous before setting any attributes.
  mTextContent->SetNativeAnonymous();

  mTextContent->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                        NS_LITERAL_STRING("text"), false);

  nsHTMLInputElement* inputElement =
    nsHTMLInputElement::FromContent(mContent);
  NS_ASSERTION(inputElement, "Why is our content not a <input>?");

  // Initialize value when we create the content in case the value was set
  // before we got here
  nsAutoString value;
  inputElement->GetDisplayFileName(value);

  nsCOMPtr<nsIDOMHTMLInputElement> textControl = do_QueryInterface(mTextContent);
  NS_ASSERTION(textControl, "Why is the <input> we created not a <input>?");
  textControl->SetValue(value);

  textControl->SetTabIndex(-1);
  textControl->SetReadOnly(true);

  if (!aElements.AppendElement(mTextContent))
    return NS_ERROR_OUT_OF_MEMORY;

  // Register the whole frame as an event listener of drag events
  mContent->AddSystemEventListener(NS_LITERAL_STRING("drop"),
                                   mMouseListener, false);
  mContent->AddSystemEventListener(NS_LITERAL_STRING("dragover"),
                                   mMouseListener, false);

  // Register as an event listener of the textbox
  // to open file dialog on mouse click
  mTextContent->AddSystemEventListener(NS_LITERAL_STRING("click"),
                                       mMouseListener, false);

  // Create the browse button
  nodeInfo = doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::input, nullptr,
                                                 kNameSpaceID_XHTML,
                                                 nsIDOMNode::ELEMENT_NODE);
  NS_NewHTMLElement(getter_AddRefs(mBrowse), nodeInfo.forget(),
                    dom::NOT_FROM_PARSER);
  if (!mBrowse)
    return NS_ERROR_OUT_OF_MEMORY;

  // Mark the element to be native anonymous before setting any attributes.
  mBrowse->SetNativeAnonymous();

  mBrowse->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                   NS_LITERAL_STRING("button"), false);

  // Create the capture button
  nsCOMPtr<nsICapturePicker> capturePicker;
  capturePicker = do_GetService("@mozilla.org/capturepicker;1");
  if (capturePicker) {
    CaptureCallbackData data;
    data.picker = capturePicker;
    data.mode = GetCaptureMode(data);

    if (data.mode != 0) {
      mCaptureMouseListener->mMode = data.mode;
      nodeInfo = doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::input, nullptr,
                                                     kNameSpaceID_XHTML,
                                                     nsIDOMNode::ELEMENT_NODE);
      NS_NewHTMLElement(getter_AddRefs(mCapture), nodeInfo.forget(),
                        dom::NOT_FROM_PARSER);
      if (!mCapture)
        return NS_ERROR_OUT_OF_MEMORY;

      // Mark the element to be native anonymous before setting any attributes.
      mCapture->SetNativeAnonymous();

      mCapture->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                        NS_LITERAL_STRING("button"), false);

      mCapture->SetAttr(kNameSpaceID_None, nsGkAtoms::value,
                        NS_LITERAL_STRING("capture"), false);

      mCapture->AddSystemEventListener(NS_LITERAL_STRING("click"),
                                       mCaptureMouseListener, false);
    }
  }
  nsCOMPtr<nsIDOMHTMLInputElement> fileContent = do_QueryInterface(mContent);
  nsCOMPtr<nsIDOMHTMLInputElement> browseControl = do_QueryInterface(mBrowse);
  if (fileContent && browseControl) {
    int32_t tabIndex;
    nsAutoString accessKey;

    fileContent->GetAccessKey(accessKey);
    browseControl->SetAccessKey(accessKey);
    fileContent->GetTabIndex(&tabIndex);
    browseControl->SetTabIndex(tabIndex);
  }

  if (!aElements.AppendElement(mBrowse))
    return NS_ERROR_OUT_OF_MEMORY;

  if (mCapture && !aElements.AppendElement(mCapture))
    return NS_ERROR_OUT_OF_MEMORY;

  // Register as an event listener of the button
  // to open file dialog on mouse click
  mBrowse->AddSystemEventListener(NS_LITERAL_STRING("click"),
                                  mMouseListener, false);

  SyncAttr(kNameSpaceID_None, nsGkAtoms::size,     SYNC_TEXT);
  SyncDisabledState();

  return NS_OK;
}

void
nsFileControlFrame::AppendAnonymousContentTo(nsBaseContentList& aElements,
                                             uint32_t aFilter)
{
  aElements.MaybeAppendElement(mTextContent);
  aElements.MaybeAppendElement(mBrowse);
  aElements.MaybeAppendElement(mCapture);
}

NS_QUERYFRAME_HEAD(nsFileControlFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
  NS_QUERYFRAME_ENTRY(nsIFormControlFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBlockFrame)

void 
nsFileControlFrame::SetFocus(bool aOn, bool aRepaint)
{
}

bool ShouldProcessMouseClick(nsIDOMEvent* aMouseEvent)
{
  // only allow the left button
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  NS_ENSURE_TRUE(mouseEvent, false);
  bool defaultPrevented = false;
  aMouseEvent->GetPreventDefault(&defaultPrevented);
  if (defaultPrevented) {
    return false;
  }

  uint16_t whichButton;
  if (NS_FAILED(mouseEvent->GetButton(&whichButton)) || whichButton != 0) {
    return false;
  }

  int32_t clickCount;
  if (NS_FAILED(mouseEvent->GetDetail(&clickCount)) || clickCount > 1) {
    return false;
  }

  return true;
}

/**
 * This is called when our capture button is clicked
 */
NS_IMETHODIMP
nsFileControlFrame::CaptureMouseListener::HandleEvent(nsIDOMEvent* aMouseEvent)
{
  nsresult rv;

  NS_ASSERTION(mFrame, "We should have been unregistered");
  if (!ShouldProcessMouseClick(aMouseEvent))
    return NS_OK;

  // Get parent nsPIDOMWindow object.
  nsIContent* content = mFrame->GetContent();
  nsHTMLInputElement* inputElement =
    nsHTMLInputElement::FromContentOrNull(content);
  if (!inputElement)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocument> doc = content->GetDocument();
  if (!doc)
    return NS_ERROR_FAILURE;

  // Get Loc title
  nsXPIDLString title;
  nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                     "MediaUpload", title);

  nsPIDOMWindow* win = doc->GetWindow();
  if (!win) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsICapturePicker> capturePicker;
  capturePicker = do_CreateInstance("@mozilla.org/capturepicker;1");
  if (!capturePicker)
    return NS_ERROR_FAILURE;

  rv = capturePicker->Init(win, title, mMode);
  NS_ENSURE_SUCCESS(rv, rv);

  // Show dialog
  uint32_t result;
  rv = capturePicker->Show(&result);
  NS_ENSURE_SUCCESS(rv, rv);
  if (result == nsICapturePicker::RETURN_CANCEL)
    return NS_OK;

  if (!mFrame) {
    // The frame got destroyed while the filepicker was up.  Don't do
    // anything here.
    // (This listener itself can't be destroyed because the event listener
    // manager holds a strong reference to us while it fires the event.)
    return NS_OK;
  }

  nsCOMPtr<nsIDOMFile> domFile;
  rv = capturePicker->GetFile(getter_AddRefs(domFile));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMArray<nsIDOMFile> newFiles;
  if (domFile) {
    newFiles.AppendObject(domFile);
  } else {
    return NS_ERROR_FAILURE;
  }

  // XXXkhuey we really should have a better UI story than the tired old
  // uneditable text box with the file name inside.
  // Set new selected files
  if (newFiles.Count()) {
    // Tell our input element that this update of the value is a user
    // initiated change. Otherwise it'll think that the value is being set by
    // a script and not fire onchange when it should.

    inputElement->SetFiles(newFiles, true);
    nsContentUtils::DispatchTrustedEvent(content->OwnerDoc(), content,
                                         NS_LITERAL_STRING("change"), true,
                                         false);
  }

  return NS_OK;
}

/**
 * This is called when we receive any registered events on the control.
 * We've only registered for drop, dragover and click events.
 */
NS_IMETHODIMP
nsFileControlFrame::BrowseMouseListener::HandleEvent(nsIDOMEvent* aEvent)
{
  NS_ASSERTION(mFrame, "We should have been unregistered");

  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.EqualsLiteral("click")) {
    if (!ShouldProcessMouseClick(aEvent))
      return NS_OK;
    
    nsHTMLInputElement* input =
      nsHTMLInputElement::FromContent(mFrame->GetContent());
    return input ? input->FireAsyncClickHandler() : NS_OK;
  }

  bool defaultPrevented = false;
  aEvent->GetPreventDefault(&defaultPrevented);
  if (defaultPrevented) {
    return NS_OK;
  }
  
  nsCOMPtr<nsIDOMDragEvent> dragEvent = do_QueryInterface(aEvent);
  if (!dragEvent || !IsValidDropData(dragEvent)) {
    return NS_OK;
  }

  if (eventType.EqualsLiteral("dragover")) {
    // Prevent default if we can accept this drag data
    aEvent->PreventDefault();
    return NS_OK;
  }

  if (eventType.EqualsLiteral("drop")) {
    aEvent->StopPropagation();
    aEvent->PreventDefault();

    nsIContent* content = mFrame->GetContent();
    NS_ASSERTION(content, "The frame has no content???");

    nsHTMLInputElement* inputElement = nsHTMLInputElement::FromContent(content);
    NS_ASSERTION(inputElement, "No input element for this file upload control frame!");

    nsCOMPtr<nsIDOMDataTransfer> dataTransfer;
    dragEvent->GetDataTransfer(getter_AddRefs(dataTransfer));

    nsCOMPtr<nsIDOMFileList> fileList;
    dataTransfer->GetFiles(getter_AddRefs(fileList));

    inputElement->SetFiles(fileList, true);
    nsContentUtils::DispatchTrustedEvent(content->OwnerDoc(), content,
                                         NS_LITERAL_STRING("change"), true,
                                         false);
  }

  return NS_OK;
}

/* static */ bool
nsFileControlFrame::BrowseMouseListener::IsValidDropData(nsIDOMDragEvent* aEvent)
{
  nsCOMPtr<nsIDOMDataTransfer> dataTransfer;
  aEvent->GetDataTransfer(getter_AddRefs(dataTransfer));
  NS_ENSURE_TRUE(dataTransfer, false);

  nsCOMPtr<nsIDOMDOMStringList> types;
  dataTransfer->GetTypes(getter_AddRefs(types));
  NS_ENSURE_TRUE(types, false);

  // We only support dropping files onto a file upload control
  bool typeSupported;
  types->Contains(NS_LITERAL_STRING("Files"), &typeSupported);
  return typeSupported;
}

nscoord
nsFileControlFrame::GetMinWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  // Our min width is our pref width
  result = GetPrefWidth(aRenderingContext);
  return result;
}

nsTextControlFrame*
nsFileControlFrame::GetTextControlFrame()
{
  nsITextControlFrame* tc = do_QueryFrame(mTextContent->GetPrimaryFrame());
  return static_cast<nsTextControlFrame*>(tc);
}

void
nsFileControlFrame::SyncAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             int32_t aWhichControls)
{
  nsAutoString value;
  if (mContent->GetAttr(aNameSpaceID, aAttribute, value)) {
    if (aWhichControls & SYNC_TEXT && mTextContent) {
      mTextContent->SetAttr(aNameSpaceID, aAttribute, value, true);
    }
    if (aWhichControls & SYNC_BUTTON && mBrowse) {
      mBrowse->SetAttr(aNameSpaceID, aAttribute, value, true);
    }
  } else {
    if (aWhichControls & SYNC_TEXT && mTextContent) {
      mTextContent->UnsetAttr(aNameSpaceID, aAttribute, true);
    }
    if (aWhichControls & SYNC_BUTTON && mBrowse) {
      mBrowse->UnsetAttr(aNameSpaceID, aAttribute, true);
    }
  }
}

void
nsFileControlFrame::SyncDisabledState()
{
  nsEventStates eventStates = mContent->AsElement()->State();
  if (eventStates.HasState(NS_EVENT_STATE_DISABLED)) {
    mTextContent->SetAttr(kNameSpaceID_None, nsGkAtoms::disabled, EmptyString(),
                          true);
    mBrowse->SetAttr(kNameSpaceID_None, nsGkAtoms::disabled, EmptyString(),
                     true);
  } else {
    mTextContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::disabled, true);
    mBrowse->UnsetAttr(kNameSpaceID_None, nsGkAtoms::disabled, true);
  }
}

NS_IMETHODIMP
nsFileControlFrame::AttributeChanged(int32_t         aNameSpaceID,
                                     nsIAtom*        aAttribute,
                                     int32_t         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::size) {
      SyncAttr(aNameSpaceID, aAttribute, SYNC_TEXT);
    } else if (aAttribute == nsGkAtoms::tabindex) {
      SyncAttr(aNameSpaceID, aAttribute, SYNC_BUTTON);
    }
  }

  return nsBlockFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

void
nsFileControlFrame::ContentStatesChanged(nsEventStates aStates)
{
  if (aStates.HasState(NS_EVENT_STATE_DISABLED)) {
    nsContentUtils::AddScriptRunner(new SyncDisabledStateEvent(this));
  }
}

bool
nsFileControlFrame::IsLeaf() const
{
  return true;
}

#ifdef DEBUG
NS_IMETHODIMP
nsFileControlFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("FileControl"), aResult);
}
#endif

nsresult
nsFileControlFrame::SetFormProperty(nsIAtom* aName,
                                    const nsAString& aValue)
{
  if (nsGkAtoms::value == aName) {
    nsCOMPtr<nsIDOMHTMLInputElement> textControl =
      do_QueryInterface(mTextContent);
    NS_ASSERTION(textControl,
                 "The text control should exist and be an input element");
    textControl->SetValue(aValue);
  }
  return NS_OK;
}      

nsresult
nsFileControlFrame::GetFormProperty(nsIAtom* aName, nsAString& aValue) const
{
  aValue.Truncate();  // initialize out param

  if (nsGkAtoms::value == aName) {
    nsHTMLInputElement* inputElement =
      nsHTMLInputElement::FromContent(mContent);

    if (inputElement) {
      inputElement->GetDisplayFileName(aValue);
    }
  }
  return NS_OK;
}

void
nsFileControlFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                     const nsRect&           aDirtyRect,
                                     const nsDisplayListSet& aLists)
{
  // box-shadow
  if (StyleBorder()->mBoxShadow) {
    aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
      nsDisplayBoxShadowOuter(aBuilder, this));
  }

  // Our background is inherited to the text input, and we don't really want to
  // paint it or out padding and borders (which we never have anyway, per
  // styles in forms.css) -- doing it just makes us look ugly in some cases and
  // has no effect in others.
  nsDisplayListCollection tempList;
  nsBlockFrame::BuildDisplayList(aBuilder, aDirtyRect, tempList);

  tempList.BorderBackground()->DeleteAll();

  // Clip height only
  nsRect clipRect(aBuilder->ToReferenceFrame(this), GetSize());
  clipRect.width = GetVisualOverflowRect().XMost();
  nscoord radii[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  OverflowClip(aBuilder, tempList, aLists, clipRect, radii);

  // Disabled file controls don't pass mouse events to their children, so we
  // put an invisible item in the display list above the children
  // just to catch events
  nsEventStates eventStates = mContent->AsElement()->State();
  if (eventStates.HasState(NS_EVENT_STATE_DISABLED) && IsVisibleForPainting(aBuilder)) {
    aLists.Content()->AppendNewToTop(
      new (aBuilder) nsDisplayEventReceiver(aBuilder, this));
  }

  DisplaySelectionOverlay(aBuilder, aLists.Content());
}

#ifdef ACCESSIBILITY
a11y::AccType
nsFileControlFrame::AccessibleType()
{
  return a11y::eHTMLFileInputType;
}
#endif

uint32_t
nsFileControlFrame::GetCaptureMode(const CaptureCallbackData& aData)
{
  int32_t filters = nsHTMLInputElement::FromContent(mContent)->GetFilterFromAccept();
  nsresult rv;
  bool captureEnabled;

  if (filters == nsIFilePicker::filterImages) {
    rv = aData.picker->ModeMayBeAvailable(nsICapturePicker::MODE_STILL,
                                             &captureEnabled);
    NS_ENSURE_SUCCESS(rv, 0);
    if (captureEnabled) {
      return nsICapturePicker::MODE_STILL;
    }
    return 0;
  }

  if (filters == nsIFilePicker::filterAudio) {
    rv = aData.picker->ModeMayBeAvailable(nsICapturePicker::MODE_AUDIO_CLIP,
                                             &captureEnabled);
    NS_ENSURE_SUCCESS(rv, 0);
    if (captureEnabled) {
      return nsICapturePicker::MODE_AUDIO_CLIP;
    }
    return 0;
  }

  if (filters == nsIFilePicker::filterVideo) {
    rv = aData.picker->ModeMayBeAvailable(nsICapturePicker::MODE_VIDEO_CLIP,
                                             &captureEnabled);
    NS_ENSURE_SUCCESS(rv, 0);
    if (captureEnabled) {
      return nsICapturePicker::MODE_VIDEO_CLIP;
    }
    rv = aData.picker->ModeMayBeAvailable(nsICapturePicker::MODE_VIDEO_NO_SOUND_CLIP,
                                             &captureEnabled);
    NS_ENSURE_SUCCESS(rv, 0);
    if (captureEnabled) {
      return nsICapturePicker::MODE_VIDEO_NO_SOUND_CLIP;
    }
    return 0;
  }
  
  return 0;
}
////////////////////////////////////////////////////////////
// Mouse listener implementation

NS_IMPL_ISUPPORTS1(nsFileControlFrame::MouseListener,
                   nsIDOMEventListener)
