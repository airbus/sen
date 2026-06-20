// === index.ts ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Public entry point for `@sen/client`.

export {
  Quantity,
  Variant,
  numberFromExact,
  type Var,
  type VarList,
  type VarMap,
  type CancelFn,
} from "./values.js";
export {
  SenClientError,
  JsonRpcError,
  JsonRpcErrorCode,
  TransportError,
  TimeoutError,
  InterestReleasedError,
} from "./errors.js";
export { connect, type ClientOptions, type ReconnectOptions, type ReportError } from "./connect.js";
export { Client, type ConnectionState, type SessionInfo } from "./client.js";
export { isPrimitive, isQuantity, isSequence, isStruct, isVariant } from "./narrowing.js";
export {
  InterestHandle,
  ObjectHandle,
  parseSenTimestamp,
  type AnyChangeHandler,
  type CallOptions,
  type DeliveryInfo,
  type EventTriggeredHandler,
  type GetObjectsBatchStateOptions,
  type GetOptions,
  type InterestState,
  type ObjectAddedHandler,
  type ObjectBatchState,
  type ObjectRemovedHandler,
  type PreSubscription,
  type PropertyChangedHandler,
  type SubscribeOptions,
} from "./handles.js";

// Curated re-exports from the generated wire types.
export type {
  AddedObjectEntry,
  AddedObjectList,
  EventTriggeredNotification,
  Identity,
  InterestUpdateNotification,
  ObjectInfo,
  ObjectInfos,
  PropertyChangedNotification,
  PropertyValueList,
  PropertyValuePair,
  SessionInfoList,
  TopologyChangedNotification,
  UnitCat,
  UnitInfo,
} from "./generated/index.js";

// Type-spec system (introspection of class / struct / variant / enum / quantity shapes).
export type {
  AliasTypeSpec,
  ArgSpec,
  ArgSpecList,
  BasicType,
  BuiltInType,
  ClassTypeSpec,
  CustomTypeData,
  CustomTypeSpec,
  CustomTypeSpecList,
  EnumTypeSpec,
  EnumeratorSpec,
  EnumeratorSpecList,
  EventSpec,
  EventSpecList,
  IntegralType,
  MethodConstnessSpec,
  MethodSpec,
  MethodSpecList,
  NumericType,
  OptionalTypeSpec,
  PropertyCategorySpec,
  PropertyRelationSpec,
  PropertySpec,
  PropertySpecList,
  QuantityTypeSpec,
  RealType,
  SequenceTypeSpec,
  StructTypeFieldSpec,
  StructTypeFieldSpecList,
  StructTypeSpec,
  TransportModeSpec,
  VariantTypeFieldSpec,
  VariantTypeFieldSpecList,
  VariantTypeSpec,
} from "./generated/index.js";
