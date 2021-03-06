/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function debugClipFlavors(aClip)
{
  let xfer = Cc["@mozilla.org/widget/transferable;1"].
             createInstance(Ci.nsITransferable);
  xfer.init(null);
  aClip.getData(xfer, Ci.nsIClipboard.kGlobalClipboard);
  let array = xfer.flavorsTransferableCanExport();
  let count = array.Count();
  info("flavors:" + count);
  for (let idx = 0; idx < count; idx++) {
    let string = array.GetElementAt(idx).QueryInterface(Ci.nsISupportsString);
    info("[" + idx + "] " + string);
  }
}

// XXX won't work with out of process content
function emptyClipboard() {
  Cc["@mozilla.org/widget/clipboard;1"].getService(Ci.nsIClipboard)
                                       .emptyClipboard(Ci.nsIClipboard.kGlobalClipboard);
}

// Image context menu tests
gTests.push({
  desc: "text context menu",
  run: function test() {
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

    info(chromeRoot + "browser_context_menu_tests_02.html");
    yield addTab(chromeRoot + "browser_context_menu_tests_02.html");

    purgeEventQueue();

    let win = Browser.selectedTab.browser.contentWindow;

    yield hideContextUI();

    ////////////////////////////////////////////////////////////
    // Context menu in content on selected text

    // select some text
    let span = win.document.getElementById("text1");
    win.getSelection().selectAllChildren(span);

    // invoke selection context menu
    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(win, span, 85, 10);
    yield promise;
    ok(promise && !(promise instanceof Error), "promise error");

    // should be visible
    ok(ContextMenuUI._menuPopup._visible, "is visible");

    // selected text context:
    checkContextUIMenuItemVisibility(["context-copy",
                                      "context-search"]);

    promise = waitForEvent(document, "popuphidden");
    ContextMenuUI.hide();
    yield promise;
    ok(promise && !(promise instanceof Error), "promise error");
    win.getSelection().removeAllRanges();

    ////////////////////////////////////////////////////////////
    // Context menu in content on selected text that includes a link

    // invoke selection with link context menu
    let link = win.document.getElementById("text2-link");
    win.getSelection().selectAllChildren(link);
    promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(win, link, 40, 10);
    yield promise;
    ok(promise && !(promise instanceof Error), "promise error");

    // should be visible
    ok(ContextMenuUI._menuPopup._visible, "is visible");

    // selected text context:
    checkContextUIMenuItemVisibility(["context-copy",
                                      "context-search",
                                      "context-open-in-new-tab",
                                      "context-copy-link"]);

    promise = waitForEvent(document, "popuphidden");
    ContextMenuUI.hide();
    yield promise;
    ok(promise && !(promise instanceof Error), "promise error");
    win.getSelection().removeAllRanges();

    ////////////////////////////////////////////////////////////
    // Context menu in content on a link

    link = win.document.getElementById("text2-link");
    promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(win, link, 40, 10);
    yield promise;
    ok(promise && !(promise instanceof Error), "promise error");

    // should be visible
    ok(ContextMenuUI._menuPopup._visible, "is visible");

    // selected text context:
    checkContextUIMenuItemVisibility(["context-open-in-new-tab",
                                      "context-copy-link",
                                      "context-bookmark-link"]);

    promise = waitForEvent(document, "popuphidden");
    ContextMenuUI.hide();
    yield promise;
    ok(promise && !(promise instanceof Error), "promise error");

    ////////////////////////////////////////////////////////////
    // context in input with no selection, no data on clipboard

    emptyClipboard();

    let input = win.document.getElementById("text3-input");
    promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(win, input, 20, 10);
    yield promise;
    ok(promise && !(promise instanceof Error), "promise error");

    // should be visible
    ok(ContextMenuUI._menuPopup._visible, "is visible");

    checkContextUIMenuItemVisibility(["context-copy",
                                      "context-select",
                                      "context-select-all"]);

    // copy menu item should copy all text
    let menuItem = document.getElementById("context-copy");
    ok(menuItem, "menu item exists");
    ok(!menuItem.hidden, "menu item visible");

    let popupPromise = waitForEvent(document, "popuphidden");
    EventUtils.synthesizeMouse(menuItem, 10, 10, {}, win);
    yield popupPromise;
    ok(popupPromise && !(popupPromise instanceof Error), "promise error");
    let string = SpecialPowers.getClipboardData("text/unicode");
    ok(string, "hello, I'm sorry but I must be going.", "copy all");

    emptyClipboard();

    ////////////////////////////////////////////////////////////
    // context in input with text selection, no data on clipboard

    input = win.document.getElementById("text3-input");
    input.select();
    promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(win, input, 20, 10);
    yield promise;
    ok(promise && !(promise instanceof Error), "promise error");

    // should be visible
    ok(ContextMenuUI._menuPopup._visible, "is visible");

    // selected text context:
    checkContextUIMenuItemVisibility(["context-copy",
                                      "context-search"]);

    promise = waitForEvent(document, "popuphidden");
    ContextMenuUI.hide();
    yield promise;
    ok(promise && !(promise instanceof Error), "promise error");

    ////////////////////////////////////////////////////////////
    // context in input with no selection, data on clipboard

    SpecialPowers.clipboardCopyString("foo");
    input = win.document.getElementById("text3-input");
    input.select();
    promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(win, input, 20, 10);
    yield promise;
    ok(promise && !(promise instanceof Error), "promise error");

    // should be visible
    ok(ContextMenuUI._menuPopup._visible, "is visible");

    // selected text context:
    checkContextUIMenuItemVisibility(["context-copy",
                                      "context-search",
                                      "context-paste"]);

    promise = waitForEvent(document, "popuphidden");
    ContextMenuUI.hide();
    yield promise;
    ok(promise && !(promise instanceof Error), "promise error");

    ////////////////////////////////////////////////////////////
    // context in empty input, data on clipboard (paste operation)

    SpecialPowers.clipboardCopyString("foo");
    input = win.document.getElementById("text3-input");
    input.value = "";

    promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(win, input, 20, 10);
    yield promise;
    ok(promise && !(promise instanceof Error), "promise error");

    // should be visible
    ok(ContextMenuUI._menuPopup._visible, "is visible");

    // selected text context:
    checkContextUIMenuItemVisibility(["context-paste"]);

    promise = waitForEvent(document, "popuphidden");
    ContextMenuUI.hide();
    yield promise;
    ok(promise && !(promise instanceof Error), "promise error");

    ////////////////////////////////////////////////////////////
    // context in empty input, no data on clipboard (??)

    emptyClipboard();

    input = win.document.getElementById("text3-input");
    input.value = "";

    promise = waitForEvent(Elements.tray, "transitionend");
    sendContextMenuClickToElement(win, input, 20, 10);
    yield promise;
    ok(promise && !(promise instanceof Error), "promise error");

    // should *not* be visible
    ok(!ContextMenuUI._menuPopup._visible, "is visible");

    // the test above will invoke the app bar
    yield hideContextUI();

    Browser.closeTab(Browser.selectedTab);
    purgeEventQueue();
  }
});

// Image context menu tests
gTests.push({
  desc: "image context menu",
  run: function test() {
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

    info(chromeRoot + "browser_context_menu_tests_01.html");
    yield addTab(chromeRoot + "browser_context_menu_tests_01.html");

    let win = Browser.selectedTab.browser.contentWindow;

    purgeEventQueue();

    yield hideContextUI();

    // If we don't do this, sometimes the first sendContextMenuClick
    // will trigger the app bar.
    yield waitForImageLoad(win, "image01");

    ////////////////////////////////////////////////////////////
    // Context menu options

    // image01 - 1x1x100x100
    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClick(win, 10, 10);
    yield promise;
    ok(promise && !(promise instanceof Error), "promise error");

    purgeEventQueue();

    ok(ContextMenuUI._menuPopup._visible, "is visible");

    checkContextUIMenuItemVisibility(["context-save-image-lib",
                                      "context-copy-image",
                                      "context-copy-image-loc",
                                      "context-open-image-tab"]);

    ////////////////////////////////////////////////////////////
    // Save to image library

    let dirSvc = Components.classes["@mozilla.org/file/directory_service;1"]
                           .getService(Components.interfaces.nsIProperties);
    let saveLocationPath = dirSvc.get("Pict", Components.interfaces.nsIFile);
    saveLocationPath.append("image01.png");

    registerCleanupFunction(function () {
      saveLocationPath.remove(false);
    });

    if (saveLocationPath.exists()) {
      info("had to remove old image!");
      saveLocationPath.remove(false);
    }

    let menuItem = document.getElementById("context-save-image-lib");
    ok(menuItem, "menu item exists");
    ok(!menuItem.hidden, "menu item visible");

    let downloadPromise = waitForObserver("dl-done");
    let popupPromise = waitForEvent(document, "popuphidden");
    EventUtils.synthesizeMouse(menuItem, 10, 10, {}, win);
    yield popupPromise;
    ok(popupPromise && !(popupPromise instanceof Error), "promise error");
    yield downloadPromise;
    ok(downloadPromise && !(downloadPromise instanceof Error), "promise error");

    purgeEventQueue();

    ok(saveLocationPath.exists(), "image saved");

    ////////////////////////////////////////////////////////////
    // Copy image

    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClick(win, 20, 20);
    yield promise;
    ok(promise && !(promise instanceof Error), "promise error");
    ok(ContextMenuUI._menuPopup._visible, "is visible");

    menuItem = document.getElementById("context-copy-image");
    ok(menuItem, "menu item exists");
    ok(!menuItem.hidden, "menu item visible");
    popupPromise = waitForEvent(document, "popuphidden");
    EventUtils.synthesizeMouse(menuItem, 10, 10, {}, win);
    yield popupPromise;
    ok(popupPromise && !(popupPromise instanceof Error), "promise error");

    purgeEventQueue();

    let clip = Cc["@mozilla.org/widget/clipboard;1"].getService(Ci.nsIClipboard);
    let flavors = ["image/png"];
    ok(clip.hasDataMatchingFlavors(flavors, flavors.length, Ci.nsIClipboard.kGlobalClipboard), "clip has my png flavor");

    ////////////////////////////////////////////////////////////
    // Copy image location

    promise = waitForEvent(document, "popupshown");
    sendContextMenuClick(win, 30, 30);
    yield promise;
    ok(promise && !(promise instanceof Error), "promise error");
    ok(ContextMenuUI._menuPopup._visible, "is visible");

    menuItem = document.getElementById("context-copy-image-loc");
    ok(menuItem, "menu item exists");
    ok(!menuItem.hidden, "menu item visible");
    popupPromise = waitForEvent(document, "popuphidden");
    EventUtils.synthesizeMouse(menuItem, 10, 10, {}, win);
    yield popupPromise;
    ok(popupPromise && !(popupPromise instanceof Error), "promise error");

    purgeEventQueue();

    let clip = Cc["@mozilla.org/widget/clipboard;1"].getService(Ci.nsIClipboard);
    let flavors = ["text/unicode"];
    ok(clip.hasDataMatchingFlavors(flavors, flavors.length, Ci.nsIClipboard.kGlobalClipboard), "clip has my text flavor");

    let xfer = Cc["@mozilla.org/widget/transferable;1"].
               createInstance(Ci.nsITransferable);
    xfer.init(null);
    xfer.addDataFlavor("text/unicode");
    clip.getData(xfer, Ci.nsIClipboard.kGlobalClipboard);
    let str = new Object();
    let strLength = new Object();
    xfer.getTransferData("text/unicode", str, strLength);
    str = str.value.QueryInterface(Components.interfaces.nsISupportsString);
    ok(str == "chrome://mochitests/content/metro/res/image01.png", "url copied");

    ////////////////////////////////////////////////////////////
    // Open image in new tab

    promise = waitForEvent(document, "popupshown");
    sendContextMenuClick(win, 40, 40);
    yield promise;
    ok(promise && !(promise instanceof Error), "promise error");
    ok(ContextMenuUI._menuPopup._visible, "is visible");

    menuItem = document.getElementById("context-open-image-tab");
    ok(menuItem, "menu item exists");
    ok(!menuItem.hidden, "menu item visible");
    let tabPromise = waitForEvent(document, "TabOpen");
    popupPromise = waitForEvent(document, "popuphidden");
    EventUtils.synthesizeMouse(menuItem, 10, 10, {}, win);
    yield popupPromise;
    let event = yield tabPromise;
    ok(popupPromise && !(popupPromise instanceof Error), "promise error");
    ok(tabPromise && !(tabPromise instanceof Error), "promise error");

    purgeEventQueue();

    let imagetab = Browser.getTabFromChrome(event.originalTarget);
    ok(imagetab != null, "tab created");
    ok(imagetab.browser.currentURI.spec == "chrome://mochitests/content/metro/res/image01.png", "tab location");

    Browser.closeTab(imagetab);
  }
});

function test() {
  runTests();
}
