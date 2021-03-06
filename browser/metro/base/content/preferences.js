// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var PreferencesPanelView = {
  init: function pv_init() {
    // Run some setup code the first time the panel is shown.
    Elements.prefsFlyout.addEventListener("PopupChanged", function onShow(aEvent) {
      if (aEvent.detail && aEvent.popup === Elements.prefsFlyout) {
        Elements.prefsFlyout.removeEventListener("PopupChanged", onShow, false);
        MasterPasswordUI.updatePreference();
        WeaveGlue.init();
      }
    }, false);
  }
};
