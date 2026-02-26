// === streaming_read_write.cpp ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "../support/reader_writer.h"

// sen
#include "sen/core/base/timestamp.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/unit.h"
#include "sen/core/meta/var.h"
#include "sen/core/meta/variant_type.h"

// generated code
#include "stl/test_io.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using sen::InputStream;
using sen::OutputStream;
using sen::test::TestReader;
using sen::test::TestWriter;

namespace test_io
{

//--------------------------------------------------------------------------------------------------------------
// Data
//--------------------------------------------------------------------------------------------------------------

auto airportCodeTypePtr()
{
  static const std::vector<sen::Enumerator> airportCodesEnumerator({{"abc", 0, "Albacete"},
                                                                    {"alc", 1, "Alicante"},
                                                                    {"lei", 2, "Lleida"},
                                                                    {"ovd", 3, "Oviedo"},
                                                                    {"bjz", 4, "Badajoz"},
                                                                    {"bcn", 5, "Barcelona"},
                                                                    {"ibz", 6, "Ibiza"},
                                                                    {"bio", 7, "Bilbao"},
                                                                    {"rjl", 8, "La Rioja"},
                                                                    {"pmi", 9, "Palma de Mallorca"},
                                                                    {"pna", 10, "Pamplona"},
                                                                    {"svq", 11, "Sevilla"},
                                                                    {"vlc", 12, "Valencia"},
                                                                    {"vgo", 13, "Vigo"},
                                                                    {"vit", 14, "Vitoria"},
                                                                    {"zaz", 15, "Zaragoza"}});
  static auto airportCodeTypePtr = sen::EnumType::make({"AirportCode",
                                                        "testio.AirportCode",
                                                        "IATA codes for spanish airports",
                                                        airportCodesEnumerator,
                                                        /*storageType=*/sen::Int16Type::get()});
  return airportCodeTypePtr;
}

std::vector<sen::Enumerator> colorCodes()
{
  static const std::vector<sen::Enumerator> colorCodes({
    {"red", 0, ""},
    {"blue", 1, ""},
    {"green", 2, ""},
    {"yellow", 3, ""},
    {"purple", 4, ""},
    {"orange", 5, ""},
  });
  return colorCodes;
}

auto colorTypePtr()
{
  static const auto colorTypePtr =
    sen::EnumType::make({"Color", "testio.Color", "", colorCodes(), sen::UInt8Type::get()});
  return colorTypePtr;
}

/// Units
auto secondsSpec()
{
  static const auto secondsSpec = sen::Unit::make({sen::UnitCategory::time, "second", "seconds", "s", 1, 0, 0});
  return secondsSpec.get();
}

auto figurePointTypePtr()
{
  static const sen::StructSpec pointSpec {
    "Point",
    "testio.Point",
    "pointed figure",
    {{"x", "x axis coordinate", sen::UInt32Type::get()}, {"y", "y axis coordinate", sen::UInt32Type::get()}}};
  static const auto figurePointTypePtr = sen::StructType::make(pointSpec);
  return figurePointTypePtr;
}

auto figurePoint3DTypePtr()
{
  static const sen::StructSpec point3dSpec {"Point3D",
                                            "testio.Point3D",
                                            "3d pointed figure",
                                            {{"z", "z axis coordinate", sen::UInt32Type::get()}},
                                            figurePointTypePtr()};
  static const auto figurePoint3DTypePtr = sen::StructType::make(point3dSpec);
  return figurePoint3DTypePtr;
}

auto figureSphereTypePtr()
{
  static const sen::StructSpec sphereSpec {"Sphere",
                                           "testio.Sphere",
                                           "sphered figure",
                                           {{"radius", "sphere radius", sen::Float64Type::get()}},
                                           figurePoint3DTypePtr()};
  static const auto figureSphereTypePtr = sen::StructType::make(sphereSpec);
  return figureSphereTypePtr;
}

auto figureHypersphereTypePtr()
{
  static const sen::StructSpec hypersphereSpec {
    "Hypersphere", "testio.Hypersphere", "hypersphered figure", {}, figureSphereTypePtr()};

  static const auto figureHypersphereTypePtr = sen::StructType::make(hypersphereSpec);
  return figureHypersphereTypePtr;
}

auto figureTypePtr()
{
  static const sen::StructSpec circleSpec {"Circle",
                                           "testio.Circle",
                                           "rounded figure",
                                           {{"radius", "circle radius", sen::Float64Type::get()}},
                                           figurePointTypePtr()};
  static const auto figureCircleTypePtr = sen::StructType::make(circleSpec);
  static const std::vector<sen::VariantField> figureVariantFields {
    {0, "point shape", figurePointTypePtr()},
    {1, "point 3d shape", figurePoint3DTypePtr()},
    {2, "circle shape", figureCircleTypePtr},
    {3, "sphere shape", figureSphereTypePtr()},
    {4, "hypersphere shape", figureHypersphereTypePtr()},
  };
  static const sen::VariantSpec figureSpec {"Figure", "testio.Figure", "", figureVariantFields};
  static const auto figureTypePtr = sen::VariantType::make(figureSpec);
  return figureTypePtr;
}

auto airportPriorityCodeTypePtr()
{
  static const auto priorityStdCodesTypePtr =
    sen::EnumType::make({"PriorityStdCode", "testio.PriorityStdCode", "", colorCodes(), sen::UInt8Type::get()});
  static const sen::StructSpec decongestSpec {
    "Decongest",
    "testio.Decongest",
    "Decongest struct",
    {{"passengersAboveLimit", "number of passengers above airport limit", sen::UInt64Type::get()}}};
  static const auto decongestTypePtr = sen::StructType::make(decongestSpec);
  static const std::vector<sen::VariantField> airportPriorityCodeFields {
    {0, "decongest priorirt", decongestTypePtr},
    {1, "standard priority code", priorityStdCodesTypePtr},
    {2, "other priorirty code", sen::UInt64Type::get()},
    {3, "is there any priority", sen::BoolType::get()},
  };

  static const auto airportPriorityCodeTypePtr =
    sen::VariantType::make({"AirportPriorityCode", "testio.AirportPriorityCode", "", airportPriorityCodeFields});
  return airportPriorityCodeTypePtr;
}

auto distanceTraveledTypePtr()
{
  static const auto localMeterUnit = sen::Unit::make({sen::UnitCategory::length, "meter", "meters", "m", 1, 0, 0});

  static const auto distanceTraveledTypePtr =
    sen::QuantityType::make({"FlightDistance",
                             "testio.FlightDistance",
                             "custom quantity for flight distance representation",
                             sen::Float32Type::get(),
                             localMeterUnit.get()});
  return distanceTraveledTypePtr;
}

/// Sequences
auto flightPilotsTypePtr()
{
  static auto flightPilotsTypePtr =
    sen::SequenceType::make({"FlightPilots", "testio.FlightPilots", "", sen::StringType::get(), 2, false});
  return flightPilotsTypePtr;
}

auto layoversTypePtr()
{
  static const auto layoversTypePtr = sen::SequenceType::make(
    {"VisitedAirports", "testio.VisitedAirports", "", airportCodeTypePtr(), std::nullopt, false});
  return layoversTypePtr;
}

auto aircraftVisitedAirportsTypePtr()
{
  static const auto aircraftVisitedAirportsTypePtr = sen::SequenceType::make(
    {"AircraftVisitedAirports", "testio.AircraftVisitedAirports", "", layoversTypePtr(), std::nullopt, false});
  return aircraftVisitedAirportsTypePtr;
}

/// Optionals
auto luggageTypePtr()
{
  static const sen::OptionalSpec luggageSpec {"MaybeLuggage", "testio.MaybeLuggage", "", sen::UInt64Type::get()};
  static const auto luggageTypePtr = sen::OptionalType::make(luggageSpec);
  return luggageTypePtr;
}

auto driftFlightDistanceTypePtr()
{
  static const sen::OptionalSpec driftFlightDistanceSpec {
    "MaybeDriftFlightDistance", "testio.MaybeDriftFlightDistance", "", distanceTraveledTypePtr()};
  static const auto driftFlightDistanceTypePtr = sen::OptionalType::make(driftFlightDistanceSpec);
  return driftFlightDistanceTypePtr;
}

auto internPilotsTypePtr()
{
  static const sen::OptionalSpec internPilotsSpec {
    "MaybeInternPilots", "testio.MaybeInternPilots", "", flightPilotsTypePtr()};
  static const auto internPilotsTypePtr = sen::OptionalType::make(internPilotsSpec);
  return internPilotsTypePtr;
}

/// Struct of variant and enums
auto simInfoTypePtr()
{
  static const sen::SequenceSpec trajectoryRepSpec {
    "TrajectoryRepresentation", "testio.TrajectoryRepresentation", "", figureTypePtr(), 2, false};
  static const auto trajectoryRepTypePtr = sen::SequenceType::make(trajectoryRepSpec);
  static const sen::OptionalSpec airportRepresentationSpec {
    "MaybeAirportRepresentation", "testio.MaybeAirportRepresentation", "", figurePointTypePtr()};
  static const auto airportRepTypePtr = sen::OptionalType::make(airportRepresentationSpec);
  static const std::vector<sen::StructField> simRepFields {{"shape", "", figureTypePtr()},
                                                           {"color", "", colorTypePtr()},
                                                           {"trajectory", "", trajectoryRepTypePtr},
                                                           {"airportRep", "", airportRepTypePtr}};
  static const auto simInfoTypePtr =
    sen::StructType::make({"SimulationRepresentation", "testio.SimulationRepresentation", "", simRepFields});
  return simInfoTypePtr;
}

/// Sequence of optionals
auto driftsFlightDistTypePtr()
{
  static const sen::SequenceSpec driftsFlightDistSpec {
    "DriftsFlightDistance", "testio.DriftsFlightDistance", "", driftFlightDistanceTypePtr()};
  static const auto driftsFlightDistTypePtr = sen::SequenceType::make(driftsFlightDistSpec);

  return driftsFlightDistTypePtr;
}

/// Arrays
auto aircraftTestCodesTypePtr()
{
  static const auto aircraftTestCodesTypePtr =
    sen::SequenceType::make({"AircraftTestCodes", "testio.AircraftTestCodes", "", sen::UInt32Type::get(), 3, true});
  return aircraftTestCodesTypePtr;
}

/// Alias
using BasicColors = test_io::Color;

auto aliasTypePtr()
{
  static const auto aliasSpec =
    sen::AliasSpec {"BasicColors", "testio.BasicColors", "alias for colors enumeration", colorTypePtr()};

  static const auto aliasTypePtr = sen::AliasType::make(aliasSpec);
  return aliasTypePtr;
}

/// Struct of multiple inheritance structs, enums, quantities, sequences, basic types, strings, timestamps and
/// durations
auto aircraftIDTypePtr() { return sen::StringType::get(); }

const auto& flightStructTypePtr()
{
  static const auto flightDurationTypePtr =
    sen::DurationType::make({"FlightDuration", "testio.FlightDuration", "", sen::UInt64Type::get(), secondsSpec()});
  static const auto takeOffTypePtr =
    sen::TimestampType::make({"TakeOff", "testio.Takeoff", "", sen::UInt64Type::get(), secondsSpec()});
  static const auto landingTypePtr =
    sen::TimestampType::make({"Landing", "testio.Landing", "", sen::UInt64Type::get(), secondsSpec()});
  static const sen::UInt64Type onboardTravelersType;
  static const sen::StructSpec flightSpec {
    "Flight",
    "testio.Flight",
    "custom struct to represent a flight",
    {{"aircraftID", "aircraft identification number", aircraftIDTypePtr()},
     {"origin", "aircraft origin airport IATA code", airportCodeTypePtr()},
     {"destination", "aircraft destination airport IATA code", airportCodeTypePtr()},
     {"distanceTraveled", "total distance travelled in meters", distanceTraveledTypePtr()},
     {"duration", "flight duration", flightDurationTypePtr},
     {"takeOff", "takeoff timestamp", takeOffTypePtr},
     {"landing", "landing timestamp", landingTypePtr},
     {"layovers", "sequence of flight stops", layoversTypePtr()},
     {"simulationInfo", "simulation info to represent the flight", simInfoTypePtr()},
     {"onboardTravelers", "number of persons aboard in the flight", sen::UInt64Type::get()},
     {"testCodes", "test codes made for the flight", aircraftTestCodesTypePtr()},
     {"flightPilots", "name of the aircraft pilots", flightPilotsTypePtr()},
     {"internPilots", "possible intern pilots aboard", internPilotsTypePtr()}}};

  static const auto flightStructTypePtr = sen::StructType::make(flightSpec);
  return flightStructTypePtr;
}

//--------------------------------------------------------------------------------------------------------------
// Tests
//--------------------------------------------------------------------------------------------------------------

/// @test
/// Checks correct transformation between custom struct generated in stl and sen var
/// @requirements(SEN-903)
TEST(TestIO, customStructVariantToFromValue)
{
  const auto flight = Flight {"m400",
                              AirportCode::vit,
                              AirportCode::ibz,
                              {410000},
                              {std::chrono::minutes(42)},
                              {sen::TimeStamp(0)},
                              {sen::TimeStamp(30)},
                              {AirportCode::pna, AirportCode::zaz, AirportCode::lei},
                              {Point3D {{60, 100}, 532}, Color::yellow, {Circle {}, Sphere {}}, Point {}},
                              89U,
                              {0xdead, 0xbeef, 0xcafe},
                              {"zipi", "zape"},
                              {}};

  auto flightVar = sen::toVariant(flight);
  auto result = sen::toValue<test_io::Flight>(flightVar);

  EXPECT_EQ(flight, result);
}

/// @test
/// Checks basic write and read functions of custom struct type from the generated code
/// @requirements(SEN-903)
TEST(TestIO, customStructReadWriteBasic)
{
  const auto flight = Flight {"m400",
                              AirportCode::bjz,
                              AirportCode::alc,
                              {265000},
                              {std::chrono::minutes(26)},
                              {sen::TimeStamp(0)},
                              {sen::TimeStamp(20)},
                              {AirportCode::pna, AirportCode::lei},
                              {Hypersphere {{60, 50, 100, 20}}, Color::yellow, {Sphere {}, Hypersphere {}}, Point3D {}},
                              78U,
                              {0xdead, 0xbeef, 0xcafe},
                              {"zipi", "zape"},
                              {}};

  TestWriter writer;
  OutputStream out(writer);
  sen::SerializationTraits<::test_io::Flight>::write(out, flight);

  TestReader reader(writer.getBuffer());
  InputStream in(reader.getBuffer());

  Flight readFlight;
  sen::SerializationTraits<::test_io::Flight>::read(in, readFlight);

  EXPECT_EQ(flight, readFlight);
}

/// @test
/// Checks defined StructSpec and type ptr for the custom stl struct
/// @requirements(SEN-903)
TEST(TestIO, customStructTypeTraits)
{

  // check general info about the flight struct type
  EXPECT_TRUE(flightStructTypePtr()->isStructType());
  EXPECT_NE(flightStructTypePtr()->asStructType(), nullptr);
  EXPECT_EQ(flightStructTypePtr()->getName(), "Flight");
  EXPECT_EQ(flightStructTypePtr()->getQualifiedName(), "testio.Flight");
  EXPECT_FALSE(flightStructTypePtr()->getParent().has_value());

  // aircraftID
  {
    auto aircraftID = flightStructTypePtr()->getFieldFromName("aircraftID");
    EXPECT_NE(aircraftID, nullptr);
  }

  // origin
  {
    auto origin = flightStructTypePtr()->getFieldFromName("origin");
    EXPECT_NE(origin, nullptr);
  }

  // destination
  {
    auto destination = flightStructTypePtr()->getFieldFromName("destination");
    EXPECT_NE(destination, nullptr);
  }

  // distanceTraveled
  {
    auto distanceTraveled = flightStructTypePtr()->getFieldFromName("distanceTraveled");
    EXPECT_NE(distanceTraveled, nullptr);
  }

  // duration
  {
    auto duration = flightStructTypePtr()->getFieldFromName("duration");
    EXPECT_NE(duration, nullptr);
  }

  // takeOff
  {
    auto takeOff = flightStructTypePtr()->getFieldFromName("takeOff");
    EXPECT_NE(takeOff, nullptr);
  }

  // landing
  {
    auto landing = flightStructTypePtr()->getFieldFromName("landing");
    EXPECT_NE(landing, nullptr);
  }

  // layovers
  {
    auto layovers = flightStructTypePtr()->getFieldFromName("layovers");
    EXPECT_NE(layovers, nullptr);
  }

  // simulationInfo
  {
    auto simulationInfo = flightStructTypePtr()->getFieldFromName("simulationInfo");
    EXPECT_NE(simulationInfo, nullptr);
  }

  // onboardTravelers
  {
    auto onboardTravelers = flightStructTypePtr()->getFieldFromName("onboardTravelers");
    EXPECT_NE(onboardTravelers, nullptr);
  }

  // testCodes
  {
    auto testCodes = flightStructTypePtr()->getFieldFromName("testCodes");
    EXPECT_NE(testCodes, nullptr);
  }

  // flightPilots
  {
    auto flightPilots = flightStructTypePtr()->getFieldFromName("flightPilots");
    EXPECT_NE(flightPilots, nullptr);
  }

  // internPilots
  {
    auto internPilots = flightStructTypePtr()->getFieldFromName("internPilots");
    EXPECT_NE(internPilots, nullptr);
  }
}

/// @test
/// Checks reading and writing of alias type
/// @requirements(SEN-903)
TEST(TestIO, aliasWriteRead)
{
  TestWriter writer;
  OutputStream out(writer);

  // write alias to out buffer
  const auto myColor = sen::toVariant(test_io::Color::purple);
  sen::impl::writeToStream(myColor, out, *aliasTypePtr());

  TestReader reader(writer.getBuffer());
  InputStream in(reader.getBuffer());

  sen::Var expectedColor;
  sen::impl::readFromStream(expectedColor, in, *aliasTypePtr());

  EXPECT_EQ(sen::toValue<test_io::BasicColors>(myColor), sen::toValue<test_io::BasicColors>(expectedColor));
}

/// @test
/// Checks reading and writing of 3-level inheritance struct type
/// @requirements(SEN-903)
TEST(TestIO, inheritedStructWriteRead)
{
  TestWriter writer;
  OutputStream out(writer);

  // write alias to out buffer
  const auto myFigure = sen::toVariant(Hypersphere {{{{1}}, 6.2732}});
  sen::impl::writeToStream(myFigure, out, *figureHypersphereTypePtr());

  TestReader reader(writer.getBuffer());
  InputStream in(reader.getBuffer());

  sen::Var expectedFigure;
  sen::impl::readFromStream(expectedFigure, in, *figureHypersphereTypePtr());

  EXPECT_EQ(sen::toValue<test_io::Hypersphere>(myFigure), sen::toValue<test_io::Hypersphere>(expectedFigure));
}

/// @test
/// Checks reading and writing of optional sequence type
/// @requirements(SEN-903)
TEST(TestIO, optionalSequenceWriteRead)
{
  TestWriter writer;
  OutputStream out(writer);

  // write alias to out buffer
  const auto interns = sen::toVariant(test_io::MaybeInternPilots {{"jose", "josefa"}});
  sen::impl::writeToStream(interns, out, *internPilotsTypePtr());

  TestReader reader(writer.getBuffer());
  InputStream in(reader.getBuffer());

  sen::Var expectedInterns;
  sen::impl::readFromStream(expectedInterns, in, *internPilotsTypePtr());

  EXPECT_EQ(sen::toValue<MaybeInternPilots>(interns), sen::toValue<MaybeInternPilots>(expectedInterns));
}

/// @test
/// Checks reading and writing of optional quantity type
/// @requirements(SEN-903)
TEST(TestIO, optionalQuantityWriteRead)
{
  // specified optional
  {
    TestWriter writer;
    OutputStream out(writer);

    // write alias to out buffer
    const auto interns = sen::toVariant(MaybeDriftFlightDistance {FlightDistance {32223}});
    sen::impl::writeToStream(interns, out, *driftFlightDistanceTypePtr());

    TestReader reader(writer.getBuffer());
    InputStream in(reader.getBuffer());

    sen::Var expectedInterns;
    sen::impl::readFromStream(expectedInterns, in, *driftFlightDistanceTypePtr());

    EXPECT_EQ(sen::toValue<MaybeDriftFlightDistance>(interns), sen::toValue<MaybeDriftFlightDistance>(expectedInterns));
  }

  // not specified optional
  {
    TestWriter writer;
    OutputStream out(writer);

    // write alias to out buffer
    const auto interns = sen::toVariant(MaybeDriftFlightDistance {});
    sen::impl::writeToStream(interns, out, *driftFlightDistanceTypePtr());

    TestReader reader(writer.getBuffer());
    InputStream in(reader.getBuffer());

    sen::Var expectedInterns;
    sen::impl::readFromStream(expectedInterns, in, *driftFlightDistanceTypePtr());

    EXPECT_EQ(sen::toValue<MaybeDriftFlightDistance>(interns), sen::toValue<MaybeDriftFlightDistance>(expectedInterns));
  }
}

/// @test
/// Checks reading and writing of sequence of optionals type
/// @requirements(SEN-903)
TEST(TestIO, sequenceOfOptionalsWriteRead)
{
  // specified optional
  {
    TestWriter writer;
    OutputStream out(writer);

    // write alias to out buffer
    const auto drifts = sen::toVariant(DriftsFlightDistance {{32323}, {83723}});
    sen::impl::writeToStream(drifts, out, *driftsFlightDistTypePtr());

    TestReader reader(writer.getBuffer());
    InputStream in(reader.getBuffer());

    sen::Var expectedDrifts;
    sen::impl::readFromStream(expectedDrifts, in, *driftsFlightDistTypePtr());

    EXPECT_EQ(sen::toValue<DriftsFlightDistance>(drifts), sen::toValue<DriftsFlightDistance>(expectedDrifts));
  }

  // not all sequence optionals specified
  {
    TestWriter writer;
    OutputStream out(writer);

    // write alias to out buffer
    const auto drifts = sen::toVariant(DriftsFlightDistance {{}, {83723}, {}, {}, {0}});
    sen::impl::writeToStream(drifts, out, *driftsFlightDistTypePtr());

    TestReader reader(writer.getBuffer());
    InputStream in(reader.getBuffer());

    sen::Var expectedDrifts;
    sen::impl::readFromStream(expectedDrifts, in, *driftsFlightDistTypePtr());

    EXPECT_EQ(sen::toValue<DriftsFlightDistance>(drifts), sen::toValue<DriftsFlightDistance>(expectedDrifts));
  }
}

/// @test
/// Checks reading and writing of sequence of sequences
/// @requirements(SEN-903)
TEST(TestIO, sequenceOfSequencesWriteRead)
{
  TestWriter writer;
  OutputStream out(writer);

  // write alias to out buffer
  const auto aircraftVisitedAirports =
    sen::toVariant(AircraftVisitedAirports {{AirportCode::pna, AirportCode::bio, AirportCode::vit},
                                            {},
                                            {AirportCode::lei, AirportCode::bcn},
                                            {AirportCode::vlc}});
  sen::impl::writeToStream(aircraftVisitedAirports, out, *aircraftVisitedAirportsTypePtr());

  TestReader reader(writer.getBuffer());
  InputStream in(reader.getBuffer());

  sen::Var expectedAircraftVisitedAirports;
  sen::impl::readFromStream(expectedAircraftVisitedAirports, in, *aircraftVisitedAirportsTypePtr());

  EXPECT_EQ(sen::toValue<AircraftVisitedAirports>(aircraftVisitedAirports),
            sen::toValue<AircraftVisitedAirports>(expectedAircraftVisitedAirports));
}

/// @test
/// Checks reading and writing of variant that combines structs with enums and native types
/// @requirements(SEN-903)
TEST(TestIO, variantCombinedTypesWriteRead)
{
  // struct type in variant
  {
    TestWriter writer;
    OutputStream out(writer);

    // write combined variant to out buffer
    Decongest prio {1200U};
    const auto airportPriority = sen::toVariant(AirportPriorityCode {prio});
    sen::impl::writeToStream(airportPriority, out, *airportPriorityCodeTypePtr());

    TestReader reader(writer.getBuffer());
    InputStream in(reader.getBuffer());

    // read and check
    sen::Var expectedAirportPriority;
    sen::impl::readFromStream(expectedAirportPriority, in, *airportPriorityCodeTypePtr());

    EXPECT_EQ(sen::toValue<AirportPriorityCode>(airportPriority),
              sen::toValue<AirportPriorityCode>(expectedAirportPriority));
  }

  // enum type in variant
  {
    TestWriter writer;
    OutputStream out(writer);

    // write combined variant to out buffer
    const auto airportPriority = sen::toVariant(AirportPriorityCode {PriorityStdCode::security});
    sen::impl::writeToStream(airportPriority, out, *airportPriorityCodeTypePtr());

    TestReader reader(writer.getBuffer());
    InputStream in(reader.getBuffer());

    // read and check
    sen::Var expectedAirportPriority;
    sen::impl::readFromStream(expectedAirportPriority, in, *airportPriorityCodeTypePtr());

    EXPECT_EQ(sen::toValue<AirportPriorityCode>(airportPriority),
              sen::toValue<AirportPriorityCode>(expectedAirportPriority));
  }
}

/// @test
/// Checks reading and writing of empty string
/// @requirements(SEN-903)
TEST(TestIO, emptyString)
{
  TestWriter writer;
  OutputStream out(writer);

  // write combined variant to out buffer
  std::string aircraftID("am400b");
  const auto varID = sen::toVariant(aircraftID);
  sen::impl::writeToStream(varID, out, *aircraftIDTypePtr());

  TestReader reader(writer.getBuffer());
  InputStream in(reader.getBuffer());

  // read and check
  sen::Var expectedID;
  sen::impl::readFromStream(expectedID, in, *aircraftIDTypePtr());

  EXPECT_EQ(sen::toValue<std::string>(varID), sen::toValue<std::string>(expectedID));
}

/// @test
/// Checks reading and writing of empty unbounded sequences
/// @requirements(SEN-903)
TEST(TestIO, emptyUnboundedSequence)
{
  TestWriter writer;
  OutputStream out(writer);

  // write combined variant to out buffer
  VisitedAirports visited {};
  const auto varVisited = sen::toVariant(visited);
  sen::impl::writeToStream(varVisited, out, *layoversTypePtr());

  TestReader reader(writer.getBuffer());
  InputStream in(reader.getBuffer());

  // read and check
  sen::Var expectedVisited;
  sen::impl::readFromStream(expectedVisited, in, *layoversTypePtr());

  EXPECT_EQ(sen::toValue<VisitedAirports>(varVisited), sen::toValue<VisitedAirports>(expectedVisited));
}

/// @test
/// Checks reading and writing of empty bounded sequences
/// @requirements(SEN-903)
TEST(TestIO, emptyBoundedSequence)
{
  TestWriter writer;
  OutputStream out(writer);

  // write combined variant to out buffer
  FlightPilots pilots {};
  const auto varPilots = sen::toVariant(pilots);
  sen::impl::writeToStream(varPilots, out, *flightPilotsTypePtr());

  TestReader reader(writer.getBuffer());
  InputStream in(reader.getBuffer());

  // read and check
  sen::Var expectedPilots;
  sen::impl::readFromStream(expectedPilots, in, *flightPilotsTypePtr());

  EXPECT_EQ(sen::toValue<FlightPilots>(varPilots), sen::toValue<FlightPilots>(expectedPilots));
}

/// @test
/// Checks reading and writing of empty array
/// @requirements(SEN-903)
TEST(TestIO, emptyArray)
{
  TestWriter writer;
  OutputStream out(writer);

  // write combined variant to out buffer
  AircraftTestCodes tests {};
  const auto varTests = sen::toVariant(tests);
  sen::impl::writeToStream(varTests, out, *aircraftTestCodesTypePtr());

  TestReader reader(writer.getBuffer());
  InputStream in(reader.getBuffer());

  // read and check
  sen::Var expectedTests;
  sen::impl::readFromStream(expectedTests, in, *aircraftTestCodesTypePtr());

  EXPECT_EQ(sen::toValue<AircraftTestCodes>(varTests), sen::toValue<AircraftTestCodes>(expectedTests));
}

/// @test
/// Checks reading and writing of empty optional
/// @requirements(SEN-903)
TEST(TestIO, emptyOptional)
{
  TestWriter writer;
  OutputStream out(writer);

  // write combined variant to out buffer
  MaybeLuggage luggage {};
  const auto varLuggage = sen::toVariant(luggage);
  sen::impl::writeToStream(varLuggage, out, *luggageTypePtr());

  TestReader reader(writer.getBuffer());
  InputStream in(reader.getBuffer());

  // read and check
  sen::Var expectedLuggage;
  sen::impl::readFromStream(expectedLuggage, in, *luggageTypePtr());

  EXPECT_EQ(sen::toValue<MaybeLuggage>(varLuggage), sen::toValue<MaybeLuggage>(expectedLuggage));
}

/// @test
/// Checks reading and writing of empty struct
/// @requirements(SEN-903)
TEST(TestIO, emptyStruct)
{
  TestWriter writer;
  OutputStream out(writer);

  // write combined variant to out buffer
  Point point {};
  const auto varPoint = sen::toVariant(point);
  sen::impl::writeToStream(varPoint, out, *figurePointTypePtr());

  TestReader reader(writer.getBuffer());
  InputStream in(reader.getBuffer());

  // read and check
  sen::Var expectedPoint;
  sen::impl::readFromStream(expectedPoint, in, *figurePointTypePtr());

  EXPECT_EQ(sen::toValue<Point>(varPoint), sen::toValue<Point>(expectedPoint));
}

/// @test
/// Checks reading and writing of empty inherited struct
/// @requirements(SEN-903)
TEST(TestIO, emptyInheritedStruct)
{
  TestWriter writer;
  OutputStream out(writer);

  // write combined variant to out buffer
  Hypersphere hypersphere {};
  const auto varHypersphere = sen::toVariant(hypersphere);
  sen::impl::writeToStream(varHypersphere, out, *figureHypersphereTypePtr());

  TestReader reader(writer.getBuffer());
  InputStream in(reader.getBuffer());

  // read and check
  sen::Var expectedHypersphere;
  sen::impl::readFromStream(expectedHypersphere, in, *figureHypersphereTypePtr());

  EXPECT_EQ(sen::toValue<Hypersphere>(varHypersphere), sen::toValue<Hypersphere>(expectedHypersphere));
}

/// @test
/// Checks reading and writing of empty variant
/// @requirements(SEN-903)
TEST(TestIO, emptyVariant)
{
  TestWriter writer;
  OutputStream out(writer);

  // write combined variant to out buffer
  Figure figure {};
  const auto varFigure = sen::toVariant(figure);
  sen::impl::writeToStream(varFigure, out, *figureTypePtr());

  TestReader reader(writer.getBuffer());
  InputStream in(reader.getBuffer());

  // read and check
  sen::Var expectedFigure;
  sen::impl::readFromStream(expectedFigure, in, *figureTypePtr());

  EXPECT_EQ(sen::toValue<Figure>(varFigure), sen::toValue<Figure>(expectedFigure));
}

/// @test
/// Checks reading and writing of empty struct that contains fields of non basic types
/// @requirements(SEN-903)
TEST(TestIO, emptyComplexStruct)
{
  TestWriter writer;
  OutputStream out(writer);

  // write combined variant to out buffer
  SimulationRepresentation simRep {};
  const auto varSimRep = sen::toVariant(simRep);
  sen::impl::writeToStream(varSimRep, out, *simInfoTypePtr());

  TestReader reader(writer.getBuffer());
  InputStream in(reader.getBuffer());

  // read and check
  sen::Var expectedSimRep;
  sen::impl::readFromStream(expectedSimRep, in, *simInfoTypePtr());

  EXPECT_EQ(sen::toValue<SimulationRepresentation>(varSimRep), sen::toValue<SimulationRepresentation>(expectedSimRep));
}

/// @test
/// Checks reading and writing of empty flight struct. The struct contains different types (inheritance struct,
/// variants, sequences, quantities...)
/// @requirements(SEN-903)
TEST(TestIO, emptyFlightStruct)
{
  TestWriter writer;
  OutputStream out(writer);

  // write combined variant to out buffer
  Flight flight {};
  const auto varFlight = sen::toVariant(flight);
  sen::impl::writeToStream(varFlight, out, *flightStructTypePtr());

  TestReader reader(writer.getBuffer());
  InputStream in(reader.getBuffer());

  // read and check
  sen::Var expectedFlight;
  sen::impl::readFromStream(expectedFlight, in, *flightStructTypePtr());

  EXPECT_EQ(sen::toValue<SimulationRepresentation>(varFlight), sen::toValue<SimulationRepresentation>(expectedFlight));
}

}  // namespace test_io
