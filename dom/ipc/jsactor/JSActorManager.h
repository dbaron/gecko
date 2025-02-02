/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSActorManager_h
#define mozilla_dom_JSActorManager_h

#include "js/TypeDecls.h"
#include "mozilla/dom/JSActor.h"
#include "mozilla/ErrorResult.h"
#include "nsString.h"

namespace mozilla {
namespace ipc {
class IProtocol;
}

namespace dom {

class JSActorProtocol;
class JSActorService;

class JSActorManager : public nsISupports {
 public:
  /**
   * Get or create an actor by it's name.
   *
   * Will return an error if the actor fails to be constructed, or `nullptr` if
   * actor creation was vetoed by a constraint.
   */
  already_AddRefed<JSActor> GetActor(const nsACString& aName, ErrorResult& aRv);

  /**
   * Handle receiving a raw message from the other side.
   */
  void ReceiveRawMessage(const JSActorMessageMeta& aMetadata,
                         ipc::StructuredCloneData&& aData,
                         ipc::StructuredCloneData&& aStack);

 protected:
  /**
   * Lifecycle methods which will fire the `willDestroy` and `didDestroy`
   * methods on relevant actors.
   */
  void JSActorWillDestroy();
  void JSActorDidDestroy();

  /**
   * Return the protocol with the given name, if it is supported by the current
   * actor.
   */
  virtual already_AddRefed<JSActorProtocol> MatchingJSActorProtocol(
      JSActorService* aActorSvc, const nsACString& aName, ErrorResult& aRv) = 0;

  /**
   * Initialize a JSActor instance given the constructed JS object.
   * `aMaybeActor` may be `nullptr`, which should construct the default empty
   * actor.
   */
  virtual already_AddRefed<JSActor> InitJSActor(JS::HandleObject aMaybeActor,
                                                const nsACString& aName,
                                                ErrorResult& aRv) = 0;

  /**
   * Return this native actor. This should be the same object which is
   * implementing `JSActorManager`.
   */
  virtual mozilla::ipc::IProtocol* AsNativeActor() = 0;

 private:
  nsRefPtrHashtable<nsCStringHashKey, JSActor> mJSActors;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_JSActorManager_h