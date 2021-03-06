/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {
  waitForExplicitFinish();

  if (!("@mozilla.org/datareporting/service;1" in Cc)) {
    ok(true, "Firefox Health Report is not enabled.");
    finish();
    return;
  }

  let reporter = Cc["@mozilla.org/datareporting/service;1"]
                   .getService()
                   .wrappedJSObject
                   .healthReporter;
  ok(reporter, "Health Reporter available.");
  reporter.onInit().then(function onInit() {
    let provider = reporter.getProvider("org.mozilla.searches");
    ok(provider, "Searches provider is available.");
    let m = provider.getMeasurement("counts", 1);

    m.getValues().then(function onData(data) {
      let now = new Date();
      let oldCount = 0;

      // This will need changed if default search engine is not Google.
      let field = "google.urlbar";

      if (data.days.hasDay(now)) {
        let day = data.days.getDay(now);
        if (day.has(field)) {
          oldCount = day.get(field);
        }
      }

      let tab = gBrowser.addTab();
      gBrowser.selectedTab = tab;

      gURLBar.value = "firefox health report";
      gURLBar.handleCommand();

      executeSoon(function afterSearch() {
        gBrowser.removeTab(tab);

        m.getValues().then(function onData(data) {
          ok(data.days.hasDay(now), "FHR has data for today.");
          let day = data.days.getDay(now);
          ok(day.has(field), "FHR has url bar count for today.");

          let newCount = day.get(field);

          is(newCount, oldCount + 1, "Exactly one search has been recorded.");
          finish();
        });
      });
    });
  });
}

