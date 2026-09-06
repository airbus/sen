// === names.cpp =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "names.h"

// std
#include <cctype>  // std::tolower
#include <cstddef>
#include <random>
#include <string>
#include <tuple>

namespace school
{

namespace
{

// clang-format off
constexpr const char* firstNames[] = {
  "James",     "John",       "Robert",      "Michael",   "William",    "David",       "Joseph",      "Richard",
  "Charles",   "Thomas",     "Christopher", "Daniel",    "Matthew",    "George",      "Donald",      "Anthony",
  "Paul",      "Mark",       "Edward",      "Steven",    "Kenneth",    "Andrew",      "Joshua",      "Brian",
  "Kevin",     "Ronald",     "Timothy",     "Jason",     "Jeffrey",    "Frank",       "Gary",        "Ryan",
  "Nicholas",  "Eric",       "Jacob",       "Stephen",   "Jonathan",   "Larry",       "Raymond",     "Scott",
  "Justin",    "Brandon",    "Gregory",     "Samuel",    "Benjamin",   "Patrick",     "Jack",        "Henry",
  "Dennis",    "Walter",     "Jerry",       "Alexander", "Tyler",      "Peter",       "Douglas",     "Harold",
  "Aaron",     "Jose",       "Adam",        "Arthur",    "Zachary",    "Nathan",      "Carl",        "Albert",
  "Kyle",      "Lawrence",   "Joe",         "Gerald",    "Willie",     "Roger",       "Keith",       "Jeremy",
  "Terry",     "Harry",      "Ralph",       "Sean",      "Jesse",      "Roy",         "Louis",       "Austin",
  "Christian", "Billy",      "Eugene",      "Bryan",     "Bruce",      "Ethan",       "Wayne",       "Russell",
  "Jordan",    "Howard",     "Fred",        "Philip",    "Alan",       "Juan",        "Randy",       "Dylan",
  "Vincent",   "Bobby",      "Johnny",      "Phillip",   "Ernest",     "Shawn",       "Clarence",    "Craig",
  "Stanley",   "Noah",       "Martin",      "Travis",    "Gabriel",    "Bradley",     "Victor",      "Leonard",
  "Earl",      "Francis",    "Jimmy",       "Todd",      "Danny",      "Cody",        "Dale",        "Carlos",
  "Logan",     "Allen",      "Alex",        "Luis",      "Frederick",  "Joel",        "Norman",      "Tony",
  "Cameron",   "Curtis",     "Rodney",      "Caleb",     "Glenn",      "Marvin",      "Alfred",      "Nathaniel",
  "Chad",      "Evan",       "Steve",       "Edwin",     "Isaac",      "Antonio",     "Melvin",      "Lee",
  "Jeffery",   "Herbert",    "Elijah",      "Luke",      "Derek",      "Ricky",       "Marcus",      "Theodore",
  "Jesus",     "Eddie",      "Mason",       "Adrian",    "Troy",       "Angel",       "Dustin",      "Mike",
  "Ian",       "Ray",        "Wesley",      "Leroy",     "Hunter",     "Jared",       "Randall",     "Bernard",
  "Lucas",     "Clifford",   "Shane",       "Jay",       "Calvin",     "Oscar",       "Jackson",     "Ronnie",
  "Corey",     "Connor",     "Leo",         "Isaiah",    "Tommy",      "Barry",       "Manuel",      "Julian",
  "Warren",    "Jon",        "Miguel",      "Jeremiah",  "Dean",       "Mitchell",    "Bill",        "Jerome",
  "Blake",     "Lloyd",      "Jayden",      "Leon",      "Brett",      "Darrell",     "Charlie",     "Seth",
  "Alvin",     "Floyd",      "Jim",         "Don",       "Micheal",    "Gavin",       "Gordon",      "Devin",
  "Aiden",     "Vernon",     "Erik",        "Trevor",    "Edgar",      "Lewis",       "Chris",       "Chase",
  "Derrick",   "Brent",      "Marc",        "Dominic",   "Clyde",      "Tom",         "Owen",        "Ricardo",
  "Max",       "Franklin",   "Mario",       "Herman",    "Lester",     "Gene",        "Elmer",       "Maurice",
  "Cory",      "Gilbert",    "Glen",        "Garrett",   "Jorge",      "Francisco",   "Clayton",     "Alejandro",
  "Landon",    "Liam",       "Cole",        "Chester",   "Sam",        "Jeff",        "Ivan",        "Colin",
  "Harvey",    "Andre",      "Jake",        "Xavier",    "Milton",     "Grant",       "Duane",       "Leslie",
  "Spencer",   "Jimmie",     "Wyatt",       "Casey",     "Reginald",   "Carter",      "Taylor",      "Roberto",
  "Sebastian", "Ruben",      "Cecil",       "Levi",      "Dan",        "Jessie",      "Oliver",      "Bryce",
  "Aidan",     "Lance",      "Preston",     "Johnnie",   "Eduardo",    "Darren",      "Arnold",      "Neil",
  "Karl",      "Hector",     "Tristan",     "Roland",    "Colton",     "Bob",         "Clinton",     "Diego",
  "Brayden",   "Darryl",     "Claude",      "Johnathan", "Allan",      "Fernando",    "Omar",        "Lonnie",
  "Everett",   "Brendan",    "Guy",         "Eli",       "Hayden",     "Tanner",      "Javier",      "Kurt",
  "Jamie",     "Riley",      "Dakota",      "Tim",       "Pedro",      "Carson",      "Andy",        "Rick",
  "Kelly",     "Brad",       "Raul",        "Wallace",   "Parker",     "Brady",       "Greg",        "Sidney",
  "Ben",       "Nicolas",    "Abraham",     "Ross",      "Dwight",     "Marshall",    "Willard",     "Tyrone",
  "Micah",     "Jackie",     "Maxwell",     "Andres",    "Dalton",     "Hugh",        "Dwayne",      "Byron",
  "Josiah",    "Ted",        "Mathew",      "Collin",    "Freddie",    "Perry",       "Nelson",      "Drew",
  "Rafael",    "Ramon",      "Nolan",       "Marion",    "Armando",    "Damian",      "Angelo",      "Devon",
  "Shaun",     "Morris",     "Stuart",      "Virgil",    "Julius",     "Cesar",       "Terrence",    "Kirk",
  "Sergio",    "Kaleb",      "Kent",        "Jaden",     "Clifton",    "Wade",        "Terrance",    "Erick",
  "Marco",     "Emmanuel",   "Dave",        "Jonathon",  "Fredrick",   "Jaime",       "Daryl",       "Luther",
  "Cooper",    "Rickey",     "Miles",       "Tracy",     "Damon",      "Kristopher",  "Homer",       "Dillon",
  "Alexis",    "Harrison",   "Donnie",      "Cristian",  "Felix",      "Earnest",     "Hubert",      "Elias",
  "Julio",     "Gerard",     "Ashton",      "Alberto",   "Wilbur",     "Malcolm",     "Lyle",        "Giovanni",
  "Enrique",   "Israel",     "Shannon",     "Caden",     "Dallas",     "Gage",        "Neal",
};

constexpr const char* surNames[] = {
  "Smith",       "Johnson",     "Williams",    "Brown",       "Jones",      "Miller",      "Davis",
  "Garcia",      "Rodriguez",   "Wilson",      "Martinez",    "Anderson",   "Taylor",      "Thomas",
  "Hernandez",   "Moore",       "Martin",      "Jackson",     "Thompson",   "White",       "Lopez",
  "Lee",         "Gonzalez",    "Harris",      "Clark",       "Lewis",      "Robinson",    "Walker",
  "Perez",       "Hall",        "Young",       "Allen",       "Sanchez",    "Wright",      "King",
  "Scott",       "Green",       "Baker",       "Adams",       "Nelson",     "Hill",        "Ramirez",
  "Campbell",    "Mitchell",    "Roberts",     "Carter",      "Phillips",   "Evans",       "Turner",
  "Torres",      "Parker",      "Collins",     "Edwards",     "Stewart",    "Flores",      "Morris",
  "Nguyen",      "Murphy",      "Rivera",      "Cook",        "Rogers",     "Morgan",      "Peterson",
  "Cooper",      "Reed",        "Bailey",      "Bell",        "Gomez",      "Kelly",       "Howard",
  "Ward",        "Cox",         "Diaz",        "Richardson",  "Wood",       "Watson",      "Brooks",
  "Bennett",     "Gray",        "James",       "Reyes",       "Cruz",       "Hughes",      "Price",
  "Myers",       "Long",        "Foster",      "Sanders",     "Ross",       "Morales",     "Powell",
  "Sullivan",    "Russell",     "Ortiz",       "Jenkins",     "Gutierrez",  "Perry",       "Butler",
  "Barnes",      "Fisher",      "Henderson",   "Coleman",     "Simmons",    "Patterson",   "Jordan",
  "Reynolds",    "Hamilton",    "Graham",      "Kim",         "Gonzales",   "Alexander",   "Ramos",
  "Wallace",     "Griffin",     "West",        "Cole",        "Hayes",      "Gibson",      "Bryant",
  "Ellis",       "Stevens",     "Murray",      "Ford",        "Marshall",   "Owens",       "Mcdonald",
  "Harrison",    "Ruiz",        "Kennedy",     "Wells",       "Alvarez",    "Woods",       "Mendoza",
  "Castillo",    "Olson",       "Webb",        "Washington",  "Tucker",     "Freeman",     "Burns",
  "Henry",       "Vasquez",     "Snyder",      "Simpson",     "Crawford",   "Jimenez",     "Porter",
  "Mason",       "Shaw",        "Gordon",      "Wagner",      "Hunter",     "Romero",      "Hicks",
  "Dixon",       "Hunt",        "Palmer",      "Robertson",   "Black",      "Holmes",      "Stone",
  "Meyer",       "Boyd",        "Mills",       "Warren",      "Fox",        "Rose",        "Rice",
  "Moreno",      "Schmidt",     "Patel",       "Ferguson",    "Nichols",    "Herrera",     "Medina",
  "Ryan",        "Fernandez",   "Weaver",      "Daniels",     "Stephens",   "Gardner",     "Payne",
  "Kelley",      "Dunn",        "Pierce",      "Arnold",      "Tran",       "Spencer",     "Peters",
  "Hawkins",     "Grant",       "Hansen",      "Castro",      "Hoffman",    "Hart",        "Elliott",
  "Cunningham",  "Knight",      "Bradley",     "Carroll",     "Hudson",     "Duncan",      "Armstrong",
  "Berry",       "Andrews",     "Johnston",    "Ray",         "Lane",       "Riley",       "Carpenter",
  "Perkins",     "Aguilar",     "Silva",       "Richards",    "Willis",     "Matthews",    "Chapman",
  "Lawrence",    "Garza",       "Vargas",      "Watkins",     "Wheeler",    "Larson",      "Carlson",
  "Harper",      "George",      "Greene",      "Burke",       "Guzman",     "Morrison",    "Munoz",
  "Jacobs",      "Obrien",      "Lawson",      "Franklin",    "Lynch",      "Bishop",      "Carr",
  "Salazar",     "Austin",      "Mendez",      "Gilbert",     "Jensen",     "Williamson",  "Montgomery",
  "Harvey",      "Howell",      "Dean",        "Hanson",      "Weber",      "Garrett",     "Sims",
  "Burton",      "Fuller",      "Soto",        "Mccoy",       "Welch",      "Chen",        "Schultz",
  "Walters",     "Reid",        "Fields",      "Walsh",       "Little",     "Fowler",      "Bowman",
  "Davidson",    "May",         "Day",         "Schneider",   "Newman",     "Brewer",      "Lucas",
  "Holland",     "Wong",        "Banks",       "Santos",      "Curtis",     "Pearson",     "Delgado",
  "Valdez",      "Pena",        "Rios",        "Douglas",     "Sandoval",   "Barrett",     "Hopkins",
  "Keller",      "Guerrero",    "Stanley",     "Bates",       "Alvarado",   "Beck",        "Ortega",
  "Wade",        "Estrada",     "Contreras",   "Barnett",     "Caldwell",   "Santiago",    "Lambert",
  "Powers",      "Chambers",    "Nunez",       "Craig",       "Leonard",    "Lowe",        "Rhodes",
  "Byrd",        "Gregory",     "Shelton",     "Frazier",     "Becker",     "Maldonado",   "Fleming",
  "Vega",        "Sutton",      "Cohen",       "Jennings",    "Parks",      "Mcdaniel",    "Watts",
  "Barker",      "Norris",      "Vaughn",      "Vazquez",     "Holt",       "Schwartz",    "Steele",
  "Benson",      "Neal",        "Dominguez",   "Horton",      "Terry",      "Wolfe",       "Hale",
  "Lyons",       "Graves",      "Haynes",      "Miles",       "Park",       "Warner",      "Padilla",
  "Bush",        "Thornton",    "Mccarthy",    "Mann",        "Zimmerman",  "Erickson",    "Fletcher",
  "Mckinney",    "Page",        "Dawson",      "Joseph",      "Marquez",    "Reeves",      "Klein",
  "Espinoza",    "Baldwin",     "Moran",       "Love",        "Robbins",    "Higgins",     "Ball",
  "Cortez",      "Le",          "Griffith",    "Bowen",       "Sharp",      "Cummings",    "Ramsey",
  "Hardy",       "Swanson",     "Barber",      "Acosta",      "Luna",       "Chandler",    "Blair",
  "Daniel",      "Cross",       "Simon",       "Dennis",      "Oconnor",    "Quinn",       "Gross",
  "Navarro",     "Moss",        "Fitzgerald",  "Doyle",       "Mclaughlin", "Rojas",       "Rodgers",
  "Stevenson",   "Singh",       "Yang",        "Figueroa",    "Harmon",     "Newton",      "Paul",
  "Manning",     "Garner",      "Mcgee",       "Reese",       "Francis",    "Burgess",     "Adkins",
  "Goodman",     "Curry",       "Brady",       "Christensen", "Potter",     "Walton",      "Goodwin",
  "Mullins",     "Molina",      "Webster",     "Campos",      "Avila",      "Sherman",     "Todd",
  "Chang",       "Blake",       "Malone",      "Wolf",        "Hodges",     "Juarez",      "Gill",
  "Farmer",      "Hines",       "Gallagher",   "Duran",       "Hubbard",    "Cannon",      "Miranda",
  "Wang",        "Saunders",    "Tate",        "Mack",        "Hammond",    "Carrillo",    "Townsend",
  "Wise",        "Ingram",      "Barton",      "Mejia",       "Ayala",      "Schroeder",   "Hampton",
  "Rowe",        "Parsons",     "Frank",       "Waters",      "Strickland", "Osborne",     "Maxwell",
  "Chan",        "Deleon",      "Norman",      "Harrington",  "Casey",      "Patton",      "Logan",
  "Bowers",      "Mueller",     "Glover",      "Floyd",       "Hartman",    "Buchanan",    "Cobb",
  "French",      "Kramer",      "Mccormick",   "Clarke",      "Tyler",      "Gibbs",       "Moody",
  "Conner",      "Sparks",      "Mcguire",     "Leon",        "Bauer",      "Norton",      "Pope",
  "Flynn",       "Hogan",       "Robles",      "Salinas",     "Yates",      "Lindsey",     "Lloyd",
  "Marsh",       "Mcbride",     "Owen",        "Solis",       "Pham",       "Lang",        "Pratt",
  "Lara",        "Brock",       "Ballard",     "Trujillo",    "Shaffer",    "Drake",       "Roman",
  "Aguirre",     "Morton",      "Stokes",      "Lamb",        "Pacheco",    "Patrick",     "Cochran",
  "Shepherd",    "Cain",        "Burnett",     "Hess",        "Li",         "Cervantes",   "Olsen",
  "Briggs",      "Ochoa",       "Cabrera",     "Velasquez",   "Montoya",    "Roth",        "Meyers",
  "Cardenas",    "Fuentes",     "Weiss",       "Hoover",      "Wilkins",    "Nicholson",   "Underwood",
  "Short",       "Carson",      "Morrow",      "Colon",       "Holloway",   "Summers",     "Bryan",
  "Petersen",    "Mckenzie",    "Serrano",     "Wilcox",      "Carey",      "Clayton",     "Poole",
  "Calderon",    "Gallegos",    "Greer",       "Rivas",       "Guerra",     "Decker",      "Collier",
  "Wall",        "Whitaker",    "Bass",        "Flowers",     "Davenport",  "Conley",      "Houston",
  "Huff",        "Copeland",    "Hood",        "Monroe",      "Massey",     "Roberson",    "Combs",
  "Franco",      "Larsen",      "Pittman",     "Randall",     "Skinner",    "Wilkinson",   "Kirby",
  "Cameron",     "Bridges",     "Anthony",     "Richard",     "Kirk",       "Bruce",       "Singleton",
  "Mathis",      "Bradford",    "Boone",       "Abbott",      "Charles",    "Allison",     "Sweeney",
  "Atkinson",    "Horn",        "Jefferson",   "Rosales",     "York",       "Christian",   "Phelps",
  "Farrell",     "Castaneda",   "Nash",        "Dickerson",   "Bond",       "Wyatt",       "Foley",
  "Chase",       "Gates",       "Vincent",     "Mathews",     "Hodge",      "Garrison",    "Trevino",
  "Villarreal",  "Heath",       "Dalton",      "Valencia",    "Callahan",   "Hensley",     "Atkins",
  "Huffman",     "Roy",         "Boyer",       "Shields",     "Lin",        "Hancock",     "Grimes",
  "Glenn",       "Cline",       "Delacruz",    "Camacho",     "Dillon",     "Parrish",     "Oneill",
  "Melton",      "Booth",       "Kane",        "Berg",        "Harrell",    "Pitts",       "Savage",
  "Wiggins",     "Brennan",     "Salas",       "Marks",       "Russo",      "Sawyer",      "Baxter",
  "Golden",      "Hutchinson",  "Liu",         "Walter",      "Mcdowell",   "Wiley",       "Rich",
  "Humphrey",    "Johns",       "Koch",        "Suarez",      "Hobbs",      "Beard",       "Gilmore",
  "Curran",      "Vinson",      "Vera",        "Clifton",     "Ervin",      "Eldridge",    "Lowry",
  "Becerra",     "Gore",        "Seymour",     "Chu",         "Field",      "Akers",       "Carrasco",
};
// clang-format on

// Shared RNG for name generation. Seeded once at startup.
std::mt19937& getRng()
{
  static std::mt19937 rng {std::random_device {}()};
  return rng;
}

}  // namespace

std::tuple<std::string, std::string, std::string> makeName()
{
  auto& rng = getRng();

  constexpr std::size_t firstNameCount = sizeof(firstNames) / sizeof(firstNames[0]);
  constexpr std::size_t surNameCount = sizeof(surNames) / sizeof(surNames[0]);

  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
  std::uniform_int_distribution<std::size_t> firstNameDist {0, firstNameCount - 1};
  std::uniform_int_distribution<std::size_t> surNameDist {0, surNameCount - 1};

  const auto* firstName = firstNames[firstNameDist(rng)];
  const auto* surName = surNames[surNameDist(rng)];
  // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)

  std::string fullName = firstName;
  fullName.append(surName);
  fullName[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(fullName[0])));

  return {firstName, surName, fullName};
}

}  // namespace school
