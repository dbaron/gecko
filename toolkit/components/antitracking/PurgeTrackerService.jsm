/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["PurgeTrackerService"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const THREE_DAYS_MS = 3 * 24 * 60 * 1000;

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gClassifier",
  "@mozilla.org/url-classifier/dbservice;1",
  "nsIURIClassifier"
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "gStorageActivityService",
  "@mozilla.org/storage/activity-service;1",
  "nsIStorageActivityService"
);

XPCOMUtils.defineLazyGetter(this, "gClassifierFeature", () => {
  return gClassifier.getFeatureByName("tracking-annotation");
});

XPCOMUtils.defineLazyGetter(this, "logger", () => {
  return console.createInstance({
    prefix: "*** PurgeTrackerService:",
    maxLogLevelPref: "privacy.purge_trackers.logging.level",
  });
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gConsiderEntityList",
  "privacy.purge_trackers.consider_entity_list"
);

this.PurgeTrackerService = function() {};

PurgeTrackerService.prototype = {
  classID: Components.ID("{90d1fd17-2018-4e16-b73c-a04a26fa6dd4}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPurgeTrackerService]),

  // Purging is batched for cookies to avoid clearing too much data
  // at once. This flag tells us whether this is the first daily iteration.
  _firstIteration: true,

  // We can only know asynchronously if a host is matched by the tracking
  // protection list, so we cache the result for faster future lookups.
  _trackingState: new Map(),
  /**
   * We use this collator to compare strings as if they were numbers.
   * Timestamps are saved as strings since the number is too large for an int.
   **/
  collator: new Intl.Collator(undefined, {
    numeric: true,
    sensitivity: "base",
  }),

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "idle-daily":
        // only allow one idle-daily listener to trigger until the list has been fully parsed.
        Services.obs.removeObserver(this, "idle-daily");
        this.purgeTrackingCookieJars();
        break;
      case "profile-after-change":
        Services.obs.addObserver(this, "idle-daily");
        break;
    }
  },

  async isTracker(principal) {
    if (principal.isNullPrincipal || principal.isSystemPrincipal) {
      return false;
    }
    let host;
    try {
      host = principal.asciiHost;
    } catch (error) {
      return false;
    }

    if (!this._trackingState.has(host)) {
      // Temporarily set to false to avoid doing several lookups if a site has
      // several subframes on the same domain.
      this._trackingState.set(host, false);

      await new Promise(resolve => {
        try {
          gClassifier.asyncClassifyLocalWithFeatures(
            principal.URI,
            [gClassifierFeature],
            Ci.nsIUrlClassifierFeature.blocklist,
            list => {
              if (list.length) {
                this._trackingState.set(host, true);
              }
              resolve();
            }
          );
        } catch {
          // Error in asyncClassifyLocalWithFeatures, it is not a tracker.
          this._trackingState.set(host, false);
          resolve();
        }
      });
    }

    return this._trackingState.get(host);
  },

  isAllowedThirdParty(firstPartyOrigin, thirdPartyHost) {
    let uri = Services.io.newURI(
      `${firstPartyOrigin}/?resource=${thirdPartyHost}`
    );
    logger.debug(`Checking entity list state for`, uri.spec);
    return new Promise(resolve => {
      try {
        gClassifier.asyncClassifyLocalWithFeatures(
          uri,
          [gClassifierFeature],
          Ci.nsIUrlClassifierFeature.entitylist,
          list => {
            let sameList = !!list.length;
            logger.debug(`Is ${uri.spec} on the entity list?`, sameList);
            resolve(sameList);
          }
        );
      } catch {
        resolve(false);
      }
    });
  },

  async maybePurgePrincipal(principal) {
    let origin = principal.origin;
    logger.debug(`Maybe purging ${origin}.`);

    // First, check if any site with that base domain had received
    // user interaction in the last N days.
    let hasInteraction = this._baseDomainsWithInteraction.has(
      principal.baseDomain
    );
    // Exit early unless we want to see if we're dealing with a tracker,
    // for telemetry.
    if (hasInteraction && !Services.telemetry.canRecordPrereleaseData) {
      logger.debug(`${origin} has user interaction, exiting.`);
      return;
    }

    // Second, confirm that we're looking at a tracker.
    let isTracker = await this.isTracker(principal);
    if (!isTracker) {
      logger.debug(`${origin} is not a tracker, exiting.`);
      return;
    }

    if (hasInteraction) {
      let expireTimeMs = this._baseDomainsWithInteraction.get(
        principal.baseDomain
      );

      // Collect how much longer the user interaction will be valid for, in hours.
      let timeRemaining = Math.floor(
        (expireTimeMs - Date.now()) / 1000 / 60 / 60 / 24
      );
      let permissionAgeHistogram = Services.telemetry.getHistogramById(
        "COOKIE_PURGING_TRACKERS_USER_INTERACTION_REMAINING_DAYS"
      );
      permissionAgeHistogram.add(timeRemaining);

      this._telemetryData.notPurged.add(principal.baseDomain);

      logger.debug(`${origin} is a tracker with interaction, exiting.`);
      return;
    }

    let isAllowedThirdParty = false;
    if (gConsiderEntityList || Services.telemetry.canRecordPrereleaseData) {
      for (let firstPartyPrincipal of this._principalsWithInteraction) {
        if (
          await this.isAllowedThirdParty(
            firstPartyPrincipal.origin,
            principal.asciiHost
          )
        ) {
          isAllowedThirdParty = true;
          break;
        }
      }
    }

    if (isAllowedThirdParty && gConsiderEntityList) {
      logger.debug(`${origin} has interaction on the entity list, exiting.`);
      return;
    }

    logger.log("Deleting data from:", origin);

    await new Promise(resolve => {
      Services.clearData.deleteDataFromPrincipal(
        principal,
        false,
        Ci.nsIClearDataService.CLEAR_ALL_CACHES |
          Ci.nsIClearDataService.CLEAR_COOKIES |
          Ci.nsIClearDataService.CLEAR_DOM_STORAGES |
          Ci.nsIClearDataService.CLEAR_SECURITY_SETTINGS |
          Ci.nsIClearDataService.CLEAR_EME |
          Ci.nsIClearDataService.CLEAR_PLUGIN_DATA |
          Ci.nsIClearDataService.CLEAR_MEDIA_DEVICES |
          Ci.nsIClearDataService.CLEAR_STORAGE_ACCESS |
          Ci.nsIClearDataService.CLEAR_AUTH_TOKENS |
          Ci.nsIClearDataService.CLEAR_AUTH_CACHE,
        resolve
      );
    });
    logger.log(`Data deleted from:`, origin);

    this._telemetryData.purged.add(principal.baseDomain);
  },

  resetPurgeList() {
    // We've reached the end of the cookies.
    // Restore the idle-daily listener so it will purge again tomorrow.
    Services.obs.addObserver(this, "idle-daily");
    // Set the date to 0 so we will start at the beginning of the list next time.
    Services.prefs.setStringPref(
      "privacy.purge_trackers.date_in_cookie_database",
      "0"
    );
  },

  submitTelemetry() {
    let { purged, notPurged, durationIntervals } = this._telemetryData;
    let now = Date.now();
    let lastPurge = Number(
      Services.prefs.getStringPref("privacy.purge_trackers.last_purge", now)
    );

    let intervalHistogram = Services.telemetry.getHistogramById(
      "COOKIE_PURGING_INTERVAL_HOURS"
    );
    let hoursBetween = Math.floor((now - lastPurge) / 1000 / 60 / 60);
    intervalHistogram.add(hoursBetween);

    Services.prefs.setStringPref(
      "privacy.purge_trackers.last_purge",
      now.toString()
    );

    let purgedHistogram = Services.telemetry.getHistogramById(
      "COOKIE_PURGING_ORIGINS_PURGED"
    );
    purgedHistogram.add(purged.size);

    let notPurgedHistogram = Services.telemetry.getHistogramById(
      "COOKIE_PURGING_TRACKERS_WITH_USER_INTERACTION"
    );
    notPurgedHistogram.add(notPurged.size);

    let duration = durationIntervals
      .map(([start, end]) => end - start)
      .reduce((acc, cur) => acc + cur, 0);

    let durationHistogram = Services.telemetry.getHistogramById(
      "COOKIE_PURGING_DURATION_MS"
    );
    durationHistogram.add(duration);
  },

  /**
   * This loops through all cookies saved in the database and checks if they are a tracking cookie, if it is it checks
   * that they have an interaction permission which is still valid. If the Permission is not valid we delete all data
   * associated with the site that owns that cookie.
   */
  async purgeTrackingCookieJars() {
    let purgeEnabled = Services.prefs.getBoolPref(
      "privacy.purge_trackers.enabled",
      false
    );
    // Only purge if ETP is enabled.
    let cookieBehavior = Services.cookies.cookieBehavior;

    let etpActive =
      cookieBehavior == Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER ||
      cookieBehavior ==
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN;

    if (!etpActive || !purgeEnabled) {
      logger.log(
        `returning early, etpActive: ${etpActive}, purgeEnabled: ${purgeEnabled}`
      );
      this.resetPurgeList();
      return;
    }
    logger.log("Purging trackers enabled, beginning batch.");
    // How many cookies to loop through in each batch before we quit
    const MAX_PURGE_COUNT = Services.prefs.getIntPref(
      "privacy.purge_trackers.max_purge_count",
      100
    );

    if (this._firstIteration) {
      this._telemetryData = {
        durationIntervals: [],
        purged: new Set(),
        notPurged: new Set(),
      };

      this._baseDomainsWithInteraction = new Map();
      this._principalsWithInteraction = [];
      for (let perm of Services.perms.getAllWithTypePrefix(
        "storageAccessAPI"
      )) {
        this._baseDomainsWithInteraction.set(
          perm.principal.baseDomain,
          perm.expireTime
        );
        this._principalsWithInteraction.push(perm.principal);
      }
    }

    // Record how long this iteration took for telemetry.
    // This is a tuple of start and end time, the second
    // part will be added at the end of this function.
    let duration = [Cu.now()];

    /**
     * We record the creationTime of the last cookie we looked at and
     * start from there next time. This way even if new cookies are added or old ones are deleted we
     * have a reliable way of finding our spot.
     **/
    let saved_date = Services.prefs.getStringPref(
      "privacy.purge_trackers.date_in_cookie_database",
      "0"
    );

    let maybeClearPrincipals = new Map();

    // TODO We only need the host name and creationTime, this gives too much info. See bug 1610373.
    let cookies = Services.cookies.cookies;

    // ensure we have only cookies that have a greater or equal creationTime than the saved creationTime.
    // TODO only get cookies that fulfill this condition. See bug 1610373.
    cookies = cookies.filter(cookie => {
      return (
        cookie.creationTime &&
        this.collator.compare(cookie.creationTime, saved_date) > 0
      );
    });

    // ensure the cookies are sorted by creationTime oldest to newest.
    // TODO get cookies in this order. See bug 1610373.
    cookies.sort((a, b) =>
      this.collator.compare(a.creationTime, b.creationTime)
    );
    cookies = cookies.slice(0, MAX_PURGE_COUNT);

    for (let cookie of cookies) {
      let httpPrincipal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
        "http://" +
          cookie.rawHost +
          ChromeUtils.originAttributesToSuffix(cookie.originAttributes)
      );
      let httpsPrincipal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
        "https://" +
          cookie.rawHost +
          ChromeUtils.originAttributesToSuffix(cookie.originAttributes)
      );
      maybeClearPrincipals.set(httpPrincipal.origin, httpPrincipal);
      maybeClearPrincipals.set(httpsPrincipal.origin, httpsPrincipal);
      saved_date = cookie.creationTime;
    }

    // We only consider recently active storage and don't batch it,
    // so only do this in the first iteration.
    if (this._firstIteration) {
      let startDate = Date.now() - THREE_DAYS_MS;
      let storagePrincipals = gStorageActivityService.getActiveOrigins(
        startDate * 1000,
        Date.now() * 1000
      );

      for (let principal of storagePrincipals.enumerate()) {
        maybeClearPrincipals.set(principal.origin, principal);
      }
    }

    for (let principal of maybeClearPrincipals.values()) {
      await this.maybePurgePrincipal(principal);
    }

    Services.prefs.setStringPref(
      "privacy.purge_trackers.date_in_cookie_database",
      saved_date
    );

    duration.push(Cu.now());
    this._telemetryData.durationIntervals.push(duration);

    // We've reached the end, no need to repeat again until next idle-daily.
    if (!cookies.length || cookies.length < 100) {
      logger.log("All cookie purging finished, resetting list until tomorrow.");
      this.resetPurgeList();
      this.submitTelemetry();
      this._firstIteration = true;
      return;
    }

    logger.log("Batch finished, queueing next batch.");
    this._firstIteration = false;
    Services.tm.idleDispatchToMainThread(() => {
      this.purgeTrackingCookieJars();
    });
  },
};
