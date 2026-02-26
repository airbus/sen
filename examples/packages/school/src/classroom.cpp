// === classroom.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/obj/object_list.h"
#include "sen/core/obj/object_source.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/school/classroom.stl.h"
#include "stl/school/student.stl.h"

// package
#include "student.h"
#include "teacher.h"

// std
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <locale>  // NOLINT(misc-include-cleaner) for std::tolower
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace school
{

std::tuple<std::string, std::string, std::string> makeName();

class ClassroomImpl: public ClassroomBase
{
  SEN_NOCOPY_NOMOVE(ClassroomImpl)

public:
  using ClassroomBase::ClassroomBase;
  ~ClassroomImpl() override
  {
    if (bus_)
    {
      bus_->remove(students_);
      bus_->remove(teacher_);

      students_.clear();
      teacher_.reset();
      bus_.reset();
    }
  }

public:
  void registered(sen::kernel::RegistrationApi& api) override
  {
    // use random numbers
    srand(time(nullptr));

    // open the bus
    bus_ = api.getSource(getStudentsBus());

    // detect students (even if they are remote)
    allStudents_ = api.selectAllFrom<StudentInterface>(getStudentsBus());

    // create the teacher
    if (getCreateTeacher())
    {
      auto [first, sur, full] = makeName();
      teacher_ = std::make_shared<TeacherImpl>(full, first, sur, allStudents_);
      setNextTeacherName(full);

      // publish the teacher
      bus_->add(teacher_);
    }

    // react to events produced by students
    std::ignore = allStudents_->list.onAdded(
      [&](const auto& iterators)
      {
        for (auto itr = iterators.typedBegin; itr != iterators.typedEnd; ++itr)
        {
          // when someone makes some noise, someone else will hear it
          auto cb = [&](const std::string& noise, float32_t volume)
          {
            students_.at(rand() % students_.size())->hearSomeNoise(noise, volume);  // NOLINT
          };

          (*itr)->onMadeSomeNoise({this, std::move(cb)}).keep();
        }
      });

    if (getDefaultSize() != 0U)
    {
      addStudents(getDefaultSize());
    }
  }

protected:
  void addStudentsImpl(uint32_t count) override
  {
    // to store the new students
    std::vector<std::shared_ptr<StudentImpl>> newStudents;
    newStudents.reserve(count);

    // create the students
    for (uint32_t i = 0U; i < count; ++i)
    {
      auto [firstName, surName, fullName] = makeName();
      newStudents.emplace_back(std::make_shared<StudentImpl>(fullName, surName, firstName));
    }

    // publish the students
    bus_->add(newStudents);
    students_.insert(students_.end(), newStudents.begin(), newStudents.end());
  }

  void removeStudentsImpl(uint32_t count) override
  {
    // to store the students to be removed
    std::vector<std::shared_ptr<StudentImpl>> toRemove;
    toRemove.reserve(count);

    for (std::size_t i = 0; i < count && !students_.empty(); ++i)
    {
      toRemove.push_back(students_.back());
      students_.pop_back();
    }

    bus_->remove(toRemove);
  }

private:
  std::shared_ptr<sen::ObjectSource> bus_;
  std::shared_ptr<TeacherImpl> teacher_;
  std::vector<std::shared_ptr<StudentImpl>> students_;
  std::shared_ptr<sen::Subscription<StudentInterface>> allStudents_;
};

SEN_EXPORT_CLASS(ClassroomImpl)

std::tuple<std::string, std::string, std::string> makeName()
{
  static const char* names[] = {
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
    "Lorenzo",   "Dominick",   "Horace",      "Brody",     "Rex",        "Otis",        "Lynn",        "Dana",
    "Rudolph",   "Enrique",    "Israel",      "Shannon",   "Caden",      "Dallas",      "Gage",        "Neal",
    "Arturo",    "Salvatore",  "Jonah",       "Wendell",   "Ayden",      "Kaden",       "Geoffrey",    "Donovan",
    "Trenton",   "Willis",     "Kerry",       "Alfredo",   "Simon",      "Alec",        "Damien",      "Bennie",
    "Kelvin",    "Trent",      "Joey",        "Darius",    "Gerardo",    "Colby",       "Garry",       "Kenny",
    "Leland",    "Leonardo",   "Bryant",      "Roman",     "Delbert",    "Edmund",      "Conner",      "Randolph",
    "Nick",      "Carlton",    "Roderick",    "Benny",     "Rudy",       "Archie",      "Forrest",     "Marcos",
    "Avery",     "Robin",      "Bryson",      "Loren",     "Rene",       "Alton",       "Emanuel",     "Josue",
    "Salvador",  "Peyton",     "Braden",      "Ty",        "Noel",       "Grady",       "Orlando",     "Jace",
    "Wilson",    "Santiago",   "Irving",      "Sammy",     "Pablo",      "Myron",       "Jaxon",       "Chance",
    "Pete",      "Ernesto",    "Wilbert",     "Morgan",    "Lowell",     "Sylvester",   "Grayson",     "Jermaine",
    "Laurence",  "Woodrow",    "Zane",        "Junior",    "Frankie",    "Abel",        "Brock",       "Cedric",
    "Dante",     "Tyson",      "Will",        "Ellis",     "Clark",      "Emmett",      "Nickolas",    "Quentin",
    "Trey",      "Skyler",     "Sherman",     "Ron",       "Irvin",      "Malachi",     "Kim",         "Saul",
    "Mack",      "Harley",     "Gustavo",     "Ervin",     "Fabian",     "Jalen",       "Roosevelt",   "Alonzo",
    "Carroll",   "Clay",       "Alfonso",     "Dewey",     "Tommie",     "Orville",     "Gregg",       "Terrell",
    "August",    "Demetrius",  "Elbert",      "Elliott",   "Rufus",      "Darin",       "Ismael",      "Kendall",
    "Bert",      "Moses",      "Braxton",     "Ken",       "Griffin",    "Billie",      "Myles",       "Marty",
    "Terence",   "Clint",      "Toby",        "Emilio",    "Graham",     "Lane",        "Cornelius",   "Hudson",
    "Elliot",    "Tucker",     "Jamal",       "Jody",      "Drake",      "Chandler",    "Bradford",    "Teddy",
    "Kayden",    "Rodolfo",    "Jayson",      "Zackary",   "Sheldon",    "Zachery",     "Jaylen",      "Merle",
    "Darrin",    "Bret",       "Dane",        "Percy",     "Camden",     "Emil",        "Conrad",      "Jasper",
    "Silas",     "Jakob",      "Dawson",      "Lamar",     "Doyle",      "Darrel",      "Wilfred",     "Dexter",
    "Asher",     "Randal",     "Stewart",     "Weston",    "Brennan",    "Lincoln",     "Hugo",        "Marlon",
    "Otto",      "Keegan",     "Desmond",     "Beau",      "Amos",       "Esteban",     "Ryder",       "Corbin",
    "Heath",     "Felipe",     "Quinn",       "Ezekiel",   "Ezra",       "Quinton",     "Guillermo",   "Lionel",
    "Aubrey",    "Branden",    "Blaine",      "Grover",    "Darnell",    "Solomon",     "Deandre",     "Lukas",
    "Timmy",     "Cayden",     "Stephan",     "Pat",       "Antoine",    "Reid",        "Kameron",     "Jerald",
    "Dewayne",   "Kai",        "Reed",        "Bennett",   "Tomas",      "Brenden",     "Easton",      "Eldon",
    "Sawyer",    "Rocky",      "Van",         "Louie",     "Mateo",      "Gilberto",    "Ed",          "Boyd",
    "Moises",    "Zachariah",  "Kyler",       "Bentley",   "Axel",       "Cade",        "Kendrick",    "Cary",
    "Kaiden",    "Freddy",     "Doug",        "Jaiden",    "Davis",      "Jarrod",      "Joaquin",     "Lamont",
    "Marquis",   "Rocco",      "Guadalupe",   "Rory",      "Payton",     "Dominique",   "Jess",        "Matt",
    "Jaxson",    "Buddy",      "Reuben",      "Ali",       "Rogelio",    "Rylan",       "Stacy",       "Amir",
    "Jude",      "Kody",       "Adan",        "Jarrett",   "Ramiro",     "Quincy",      "Edmond",      "Garland",
    "Vance",     "Burton",     "Courtney",    "Jan",       "Charley",    "Darwin",      "Rodrigo",     "Rolando",
    "Loyd",      "Royce",      "Johnie",      "Landen",    "Harlan",     "Zion",        "Rodger",      "Jean",
    "Murray",    "Isiah",      "Robbie",      "Jonas",     "Anton",      "Brendon",     "Norbert",     "Roscoe",
    "Bailey",    "Nikolas",    "Conor",       "Elwood",    "Noe",        "Winston",     "Erwin",       "Declan",
    "Tristen",   "Elton",      "Phil",        "Mauricio",  "Issac",      "Denis",       "Kristian",    "Marlin",
    "Nathanael", "Bobbie",     "Thaddeus",    "Walker",    "Sammie",     "Chuck",       "Scotty",      "Zackery",
    "Keaton",    "Derick",     "Dorian",      "Xander",    "Stefan",     "Maddox",      "Jefferson",   "Kurtis",
    "Dion",      "Emerson",    "Duncan",      "Rickie",    "Curt",       "Elvin",       "Rusty",       "Holden",
    "Cruz",      "Maximus",    "Skylar",      "Vicente",   "Bart",       "Reynaldo",    "Javon",       "Aden",
    "Millard",   "Vaughn",     "Cyrus",       "Carey",     "Pierre",     "Damion",      "Leonel",      "Ariel",
    "Keenan",    "Jarvis",     "Emiliano",    "Norris",    "Kermit",     "Aron",        "Donte",       "Ahmad",
    "Luca",      "Jayce",      "Rashad",      "Galen",     "Jeffry",     "Greyson",     "Elvis",       "Ernie",
    "Josh",      "Ashley",     "Raphael",     "Gus",       "Hal",        "Khalil",      "Thurman",     "Marcel",
    "Zander",    "Reece",      "Tyrell",      "Carmen",    "Deshawn",    "Tate",        "Russel",      "Al",
    "Irwin",     "Kobe",       "Justice",     "Efrain",    "Tobias",     "Darian",      "Uriel",       "Scot",
    "Humberto",  "Gael",       "Stacey",      "Coleman",   "Milo",       "Jamar",       "Ignacio",     "Emery",
    "Monty",     "Kory",       "Armand",      "Trevon",    "Kris",       "Barney",      "Shelby",      "Romeo",
    "Jarred",    "Denny",      "Maynard",     "Merlin",    "Aldo",       "Delmar",      "Ned",         "Reese",
    "Wiley",     "Bryon",      "Caiden",      "Quintin",   "Kellen",     "Odell",       "Jamison",     "Sonny",
    "Brenton",   "Devan",      "Titus",       "Ward",      "Weldon",     "Brayan",      "Ronny",       "Kareem",
    "Camron",    "Emory",      "Tyree",       "Brice",     "Garret",     "Adolfo",      "Joesph",      "Wilmer",
    "Deon",      "Sanford",    "Blair",       "Braylon",   "Braeden",    "Gerry",       "Waylon",      "Ollie",
    "Coy",       "Mohamed",    "Kade",        "Kingston",  "Cash",       "Alvaro",      "Forest",      "Kirby",
    "Mary",      "Colten",     "Barrett",     "Bruno",     "Gale",       "Maximilian",  "Osvaldo",     "Judah",
    "Agustin",   "Seymour",    "Markus",      "Jordon",    "Ulysses",    "Donny",       "Mitchel",     "Houston",
    "Stevie",    "Morton",     "Pierce",      "Chadwick",  "Frederic",   "Davon",       "Asa",         "Laverne",
    "Braydon",   "Nehemiah",   "Jarod",       "Cullen",    "Dayton",     "Donnell",     "Clement",     "Truman",
    "Merrill",   "Demarcus",   "Rhett",       "Alden",     "Hollis",     "Abram",       "Hans",        "Colt",
    "Carlo",     "Harris",     "Isaias",      "Rowan",     "Vito",       "Linwood",     "Porter",      "Ezequiel",
    "Amari",     "Nigel",      "Ryker",       "Augustus",  "Erich",      "Orion",       "Clair",       "Jase",
    "Devonte",   "Tevin",      "Addison",     "Davion",    "Finn",       "Korey",       "Jaron",       "Prince",
    "Antwan",    "Kasey",      "Eddy",        "Paxton",    "Gunnar",     "Buford",      "Earle",       "Vince",
    "Cleo",      "Reggie",     "Basil",       "Deven",     "Scottie",    "Dirk",        "Robby",       "Triston",
    "Royal",     "Major",      "Denver",      "Elisha",    "Jaquan",     "Wilburn",     "Mervin",      "Marquise",
    "Daren",     "River",      "Cristopher",  "Kieran",    "Kolby",      "Benito",      "Brantley",    "Cornell",
    "Jamel",     "Fletcher",   "Jamari",      "Booker",    "Phoenix",    "Shayne",      "Kolton",      "Trace",
    "Jerrod",    "Dusty",      "Cyril",       "Elmo",      "Jett",       "Maximiliano", "Kristofer",   "Ari",
    "Nasir",     "Kennith",    "Lanny",       "Nathanial", "Denzel",     "Claud",       "Carmine",     "Randell",
    "Yahir",     "Gideon",     "Alva",        "Chaz",      "Darien",     "Dudley",      "Jadon",       "Lavern",
    "Rolland",   "Octavio",    "Bradly",      "Domingo",   "Tristin",    "Jaylon",      "Maverick",    "Jaylin",
    "Kane",      "Davin",      "Layne",       "Talon",     "Alphonse",   "Ibrahim",     "Shirley",     "Vern",
    "Erin",      "Cohen",      "Pasquale",    "Shelton",   "Ulises",     "Wilford",     "Gino",        "Sandy",
    "Karson",    "Rod",        "Herschel",    "Zayden",    "Michel",     "Johan",       "Brennen",     "Unknown",
    "Everette",  "Cordell",    "Jairo",       "Mikel",     "Tracey",     "Antony",      "Fidel",       "Rigoberto",
    "Izaiah",    "Jacoby",     "Dannie",      "Rob",       "Tyrese",     "Hershel",     "Keven",       "Winfred",
    "Lenard",    "Heriberto",  "Raymundo",    "Julien",    "Coby",       "Bernardo",    "Gail",        "Cortez",
    "Brycen",    "Zechariah",  "Bo",          "Nestor",    "Jamaal",     "Theron",      "Brodie",      "Hassan",
    "Alonso",    "Carol",      "Hank",        "Sage",      "Brant",      "Devante",     "Barton",      "Tylor",
    "Rodrick",   "Stan",       "German",      "Gianni",    "Luciano",    "Gonzalo",     "Marques",     "Freeman",
    "Cliff",     "Remington",  "Danial",      "Patsy",     "Lon",        "Daquan",      "Rohan",       "Rico",
    "Fredric",   "Stephon",    "Nico",        "Milford",   "Gregorio",   "Jerrold",     "Cale",        "Estevan",
    "Mekhi",     "Bud",        "Tod",         "Mackenzie", "Domenic",    "Darrick",     "Orval",       "Dino",
    "Austen",    "Jax",        "Matteo",      "Foster",    "Jaydon",     "Lawson",      "Garth",       "Lyman",
    "Dashawn",   "Chaim",      "Josef",       "Dario",     "Armani",     "Dominik",     "Augustine",   "Donavan",
    "Kip",       "Kenton",     "Judson",      "Jabari",    "Arron",      "Deonte",      "Paris",       "Valentin",
    "Keon",      "Eliseo",     "Kole",        "Jovan",     "Odis",       "Lucian",      "Jed",         "Adriel",
    "Bronson",   "Errol",      "Beckett",     "Dandre",    "Olin",       "Carmelo",     "Shea",        "Landyn",
    "Wilton",    "Misael",     "Jacques",     "Marcelo",   "Alijah",     "Bernie",      "Delmer",      "Kenyon",
    "Lyndon",    "Giancarlo",  "Wilfredo",    "Ellsworth", "Ferdinand",  "Sydney",      "Jerold",      "Buster",
    "Cletus",    "Darrius",    "Alessandro",  "Thad",      "Dangelo",    "Kale",        "Tory",        "Aric",
    "Dillan",    "Samir",      "Raheem",      "Ryland",    "Braylen",    "Broderick",   "Elroy",       "Ronan",
    "Jamey",     "Darion",     "Dimitri",     "Dwain",     "Keagan",     "Vincenzo",    "Justus",      "Kason",
    "Omari",     "Abe",        "Enoch",       "Adonis",    "Chauncey",   "Jerod",       "Omer",        "Alexandro",
    "Bertram",   "Enzo",       "Ivory",       "Edison",    "Samson",     "Nash",        "Lemuel",      "Baby",
    "Noble",     "Nicky",      "Hoyt",        "Efren",     "Mckinley",   "Jamarion",    "Emmitt",      "Yosef",
    "Layton",    "Tad",        "Emile",       "Santino",   "Karter",     "Andreas",     "Jorden",      "Newton",
    "Burl",      "Adrien",     "Devyn",       "Lucius",    "Theo",       "Cason",       "Madison",     "Lucien",
    "Kellan",    "Daron",      "Rhys",        "Travon",    "Mariano",    "Garrison",    "Raleigh",     "Arjun",
    "Donn",      "Sol",        "Tristian",    "Connie",    "Lenny",      "Huey",        "Antwon",      "Demario",
    "Seamus",    "Kraig",      "Braiden",     "Rey",       "Benedict",   "Rashawn",     "Blaise",      "Normand",
    "Hyman",     "Mathias",    "Olen",        "Cristobal", "Stanford",   "Turner",      "Giovani",     "Coty",
    "Darron",    "Reagan",     "Giovanny",    "Leif",      "Shaquille",  "Cedrick",     "Xzavier",     "Kamron",
    "Uriah",     "Isidro",     "Akeem",       "Jacky",     "Lindsey",    "Jules",       "Mose",        "Tyron",
    "Tariq",     "Francesco",  "Matias",      "Korbin",    "Alexzander", "Keshawn",     "Atticus",     "Brandan",
    "Genaro",    "Quinten",    "Zack",        "Demond",    "Arlen",      "Abdullah",    "Valentino",   "Milan",
    "Ambrose",   "Jerrell",    "Jovani",      "Kennedy",   "Carrol",     "Jamil",       "Federico",    "Shamar",
    "Palmer",    "Tre",        "Isai",        "Trever",    "Kamden",     "Simeon",      "Matthias",    "Jedidiah",
    "Benton",    "Draven",     "Rogers",      "Lonny",     "Cannon",     "Mac",         "Iker",        "Warner",
    "Dewitt",    "Jamarcus",   "Hobert",      "Russ",      "Zakary",     "Kian",        "Jade",        "Camren",
    "Westley",   "Koby",       "Niko",        "Dereck",    "Kamari",     "Nathen",      "Eliezer",     "Deshaun",
    "Sullivan",  "Rubin",      "Arden",       "Jair",      "Yusuf",      "Mitch",       "Jensen",      "Oswaldo",
    "Blaze",     "Raymon",     "Hamza",       "Oren",      "Benson",     "Napoleon",    "Maximillian", "Otha",
    "Marcellus", "Jennifer",   "Andrea",      "Franco",    "Abner",      "Boston",      "Arlie",       "Douglass",
    "Javion",    "Kelton",     "Eliot",       "Val",       "Jeromy",     "Deron",       "Burt",        "Deion",
    "Keyon",     "Dax",        "Finnegan",    "Semaj",     "Felton",     "Hilton",      "Nikhil",      "Dontae",
    "Kelley",    "Joan",       "Whitney",     "Dallin",    "Chet",       "Kadin",       "Edgardo",     "Kylan",
    "Torrey",    "Harmon",     "Wilber",      "Derik",     "Leighton",   "Ean",         "Marcelino",   "Abdul",
    "Menachem",  "Lupe",       "Art",         "Dejuan",    "Ace",        "Jaeden",      "Malakai",     "Edwardo",
    "Lacy",      "Garrick",    "Brandyn",     "Verne",     "Lazaro",     "Kevon",       "Aydan",       "Shad",
    "Tyshawn",   "Finley",     "Leandro",     "Brandt",    "Branson",    "Jaren",       "Garett",      "Kalvin",
    "Elden",     "Richie",     "Destin",      "Nikolai",   "Deacon",     "Eloy",        "Gavyn",       "Sincere",
    "Baron",     "Amare",      "Stone",       "Jevon",     "Dee",        "Jeramy",      "Mustafa",     "Zain",
    "Jagger",    "Johnpaul",   "Toney",       "Isadore",   "Lars",       "Talmadge",    "Davian",      "Ronaldo",
    "Jamir",     "Patricia",   "Gayle",       "Davonte",   "Britton",    "Yehuda",      "Aloysius",    "Archer",
    "Donavon",   "Infant",     "Anson",       "Case",      "Ora",        "Zayne",       "Zavier",      "Demarco",
    "Miller",    "Junius",     "Shon",        "Channing",  "Walton",     "Cain",        "Aditya",      "Anders",
    "Brain",     "Jovanni",    "Knox",        "Hakeem",    "Soren",      "Kasen",       "Rylee",       "Edd",
    "Stanton",   "Dickie",     "Jordyn",      "Valentine", "Rupert",     "Jerimiah",    "Maximo",      "Arnulfo",
    "Austyn",    "Jovany",     "Ishmael",     "Chace",     "Hezekiah",   "Killian",     "Ramsey",      "Rasheed",
    "Aurelio",   "Harland",    "Zaire",       "Sherwood",  "Teagan",     "Haywood",     "Ladarius",    "Mordechai",
    "Kelsey",    "Arther",     "Casimir",     "Derrell",   "Gearld",     "Johnson",     "Christophe",  "Ryne",
    "Glynn",     "Jeremie",    "Mauro",       "Tobin",     "Waldo",      "Granville",   "Vladimir",    "Antione",
    "Yisroel",   "Jovanny",    "Kimberly",    "Jaylan",    "Tye",        "Ephraim",     "Justyn",      "Arlo",
    "Markell",   "Wally",      "Keanu",       "Luka",      "Aaden",      "Jordy",       "Greggory",    "Lauren",
    "Jewel",     "Dock",       "Alexandre",   "Elian",     "Laron",      "Duke",        "Jessy",       "Kristoffer",
    "Boyce",     "Maxim",      "Jerad",       "Jasen",     "Keyshawn",   "Reilly",      "Aydin",       "Bilal",
    "Rayford",   "Syed",       "Britt",       "Rollin",    "Dylon",      "Konner",      "Jeramiah",    "Savion",
    "Bradyn",    "Damarion",   "Justen",      "Marcello",  "Aryan",      "Jaleel",      "Melvyn",      "Brannon",
    "Zaiden",    "Trevin",     "Darrion",     "Dorsey",    "Antone",     "Jessica",     "Garfield",    "Elwin",
    "Hernan",    "Gaven",      "Angus",       "Bridger",   "Kyree",      "Brennon",     "Raiden",      "Ameer",
    "Dequan",    "Kash",       "Konnor",      "Leigh",     "Camilo",     "Gaige",       "Harper",      "Braulio",
    "Fritz",     "Immanuel",   "Taj",         "Barbara",   "Lucio",      "Dameon",      "Eldridge",    "Kolten",
    "Samual",    "Jered",      "Jewell",      "Gustave",   "Tremaine",   "Judd",        "Juwan",       "Nevin",
    "Tremayne",  "Karim",      "Slade",       "Eden",      "Yousef",     "Meyer",       "Hayes",       "Merton",
    "Markel",    "Deric",      "Eleazar",     "Fay",       "Bishop",     "Jaheim",      "Lawerence",   "Curtiss",
    "Kendell",   "Gannon",     "Everardo",    "Winfield",  "Frances",    "Memphis",     "Stetson",     "Mikael",
    "Giuseppe",  "Ottis",      "Talan",       "Buck",      "Dillion",    "Kalen",       "Trystan",     "Kirt",
    "Delvin",    "Rich",       "Del",         "Franklyn",  "Daxton",     "Kelby",       "Javonte",     "Terrill",
    "Yaakov",    "Bowen",      "Braedon",     "Shay",      "Latrell",    "Jere",        "Omarion",     "Betty",
    "Westin",    "Sami",       "Montrell",    "Ajay",      "Ethen",      "Deondre",     "Delano",      "Arian",
    "Kaeden",    "Maxie",      "Emmet",       "Ford",      "Jasiah",     "Berry",       "Linda",       "Rayan",
    "Von",       "Merritt",    "Johann",      "Cristofer", "Lindsay",    "Beverly",     "Makai",       "Camryn",
    "Reyes",     "Haden",      "Kendal",      "Lorne",     "Tyrus",      "Ritchie",     "Carleton",    "Brogan",
    "Jelani",    "Gauge",      "Rishi",       "Trae",      "Zaid",       "Donell",      "Constantine", "Fredy",
    "Codey",     "Shmuel",     "Kahlil",      "Kegan",     "Claudio",    "Tavon",       "Tyrel",       "Jeramie",
    "Alford",    "Jadyn",      "Fisher",      "Kyron",     "Rosario",    "Santana",     "Waymon",      "Alvis",
    "Gil",       "Elizabeth",  "Leopoldo",    "Edsel",     "Lesley",     "Murphy",      "Rahul",       "Cassius",
    "Jefferey",  "Hardy",      "Tarik",       "Artis",     "Alfonzo",    "Timmothy",    "Dorothy",     "Jericho",
    "Schuyler",  "Hermon",     "Rian",        "Ike",       "Kay",        "Tyquan",      "Adalberto",   "Blane",
    "Elmore",    "Colter",     "Domenick",    "Malcom",    "Lynwood",    "Torin",       "Dajuan",      "Hamilton",
    "Isaak",     "Remy",       "Cal",         "Dwaine",    "Abdiel",     "Spenser",     "Geovanni",    "Regis",
    "Winford",   "Michelle",   "Yandel",      "Kenyatta",  "Pearl",      "Santo",       "Orrin",       "Amarion",
    "Pranav",    "Derwin",     "Donta",       "Cassidy",   "Callum",     "Horacio",     "Jarett",      "Treyvon",
    "Woody",     "Madden",     "Darell",      "Chip",      "Christofer", "Wes",         "Hobart",      "Cael",
    "Jonatan",   "Torrance",   "Hayward",     "Malaki",    "Les",        "Jarrell",     "Zeke",        "Lennon",
    "Krish",     "Deontae",    "Glendon",     "Mahlon",    "Shlomo",     "Yitzchok",    "Dedrick",     "Shiloh",
    "Ely",       "Haskell",    "Wardell",     "Eamon",     "Kamren",     "Aarav",       "Aidyn",       "Len",
    "Jayvon",    "Randel",     "Rayshawn",    "Kwame",     "Clarance",   "Landry",      "Maria",       "Orin",
    "Aedan",     "Verlin",     "Jeremey",     "Marko",     "Yair",       "Amar",        "Loy",         "Merlyn",
    "Sylas",     "Theadore",   "Joseluis",    "Dyllan",    "Devonta",    "Martez",      "Sebastien",   "Montana",
    "Ryley",     "Blayne",     "Beckham",     "Albin",     "Leander",    "Meredith",    "Reynold",     "Paulo",
    "Butch",     "Sedrick",    "Jayme",       "Trevion",   "Jaycob",     "Hasan",       "Robb",        "Rosendo",
    "Campbell",  "Darold",     "Tatum",       "Nate",      "Langston",   "Arnav",       "Odin",        "Stevan",
    "Auston",    "Marcell",    "Trenten",     "Nicklaus",  "Vidal",      "Devlin",      "Demetri",     "Gibson",
    "Dakotah",   "Jaydin",     "Haiden",      "Jamin",     "Jerel",      "Kayson",      "Nikko",       "Canaan",
    "Dilan",     "Kentrell",   "Demarion",    "Rashaad",   "Darrian",    "Bernice",     "Lisa",        "Donal",
    "Jacobi",    "Nicholaus",  "Desean",      "Jaxton",    "Javan",      "Mel",         "Tripp",       "Kamryn",
    "Avi",       "Farrell",    "Loyal",       "Derrek",    "Arman",      "Deanthony",   "Kenji",       "Adrain",
    "Jarret",    "Darrien",    "Marley",      "Luc",       "Dayne",      "Jakobe",      "Dillard",     "Alvie",
    "Izayah",    "Trinidad",   "Dionte",      "Kael",      "Audie",      "Lorin",       "Oran",        "Cortney",
    "Levon",     "Bonnie",     "Brien",       "Paolo",     "Thor",       "Shimon",      "Barron",      "Dakoda",
    "Obie",      "Fermin",     "Damarcus",    "Garnett",   "Ridge",      "Norberto",    "Eldred",      "Jermey",
    "Raven",     "Jai",        "Doris",       "Kenan",     "Treyton",    "Marshal",     "Trayvon",     "Elzie",
    "Artie",     "Timmie",     "Arnoldo",     "Mikal",     "Alek",       "Callen",      "Sabastian",   "Jaxen",
    "Brook",     "Montgomery", "Giles",       "Casper",    "Jaidyn",     "Isreal",      "Kyan",        "Clem",
    "Gerson",    "Kyson",      "Avraham",     "Ammon",     "Anish",      "Carlyle",     "Lashawn",     "Elwyn",
    "Torey",     "Christoper", "Jennings",    "Merrick",   "Jayceon",    "Storm",       "Jaret",       "Early",
    "Makhi",     "Leeroy",     "Yael",        "Alain",     "Arvin",      "Lavon",       "Gian",        "Jakub",
    "Lathan",    "Linus",      "Kadyn",       "Bertrand",  "Kymani",     "Gabe",        "Isidore",     "Abelardo",
    "Codie",     "Casen",      "Terell",      "Cleve",     "Isac",       "Gareth",      "Florian",     "Margarito",
    "Jameel",    "Javen",      "Oakley",      "Keandre",   "Gaylon",     "Kye",         "Rayden",      "Corban"};

  static const char* surnames[] = {
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
    "Ibarra",      "Keith",       "Macias",      "Khan",        "Andrade",    "Ware",        "Stephenson",
    "Henson",      "Wilkerson",   "Dyer",        "Mcclure",     "Blackwell",  "Mercado",     "Tanner",
    "Eaton",       "Clay",        "Barron",      "Beasley",     "Oneal",      "Preston",     "Small",
    "Wu",          "Zamora",      "Macdonald",   "Vance",       "Snow",       "Mcclain",     "Stafford",
    "Orozco",      "Barry",       "English",     "Shannon",     "Kline",      "Jacobson",    "Woodard",
    "Huang",       "Kemp",        "Mosley",      "Prince",      "Merritt",    "Hurst",       "Villanueva",
    "Roach",       "Nolan",       "Lam",         "Yoder",       "Mccullough", "Lester",      "Santana",
    "Valenzuela",  "Winters",     "Barrera",     "Leach",       "Orr",        "Berger",      "Mckee",
    "Strong",      "Conway",      "Stein",       "Whitehead",   "Bullock",    "Escobar",     "Knox",
    "Meadows",     "Solomon",     "Velez",       "Odonnell",    "Kerr",       "Stout",       "Blankenship",
    "Browning",    "Kent",        "Lozano",      "Bartlett",    "Pruitt",     "Buck",        "Barr",
    "Gaines",      "Durham",      "Gentry",      "Mcintyre",    "Sloan",      "Melendez",    "Rocha",
    "Herman",      "Moon",        "Hendricks",   "Rangel",      "Stark",      "Lowery",      "Hardin",
    "Hull",        "Sellers",     "Ellison",     "Calhoun",     "Gillespie",  "Mora",        "Knapp",
    "Mccall",      "Morse",       "Dorsey",      "Weeks",       "Nielsen",    "Livingston",  "Leblanc",
    "Mclean",      "Bradshaw",    "Glass",       "Middleton",   "Buckley",    "Schaefer",    "Frost",
    "Howe",        "House",       "Mcintosh",    "Ho",          "Pennington", "Reilly",      "Hebert",
    "Mcfarland",   "Hickman",     "Noble",       "Spears",      "Conrad",     "Arias",       "Galvan",
    "Velazquez",   "Huynh",       "Frederick",   "Randolph",    "Cantu",      "Fitzpatrick", "Mahoney",
    "Peck",        "Villa",       "Michael",     "Donovan",     "Mcconnell",  "Walls",       "Boyle",
    "Mayer",       "Zuniga",      "Giles",       "Pineda",      "Pace",       "Hurley",      "Mays",
    "Mcmillan",    "Crosby",      "Ayers",       "Case",        "Bentley",    "Shepard",     "Everett",
    "Pugh",        "David",       "Mcmahon",     "Dunlap",      "Bender",     "Hahn",        "Harding",
    "Acevedo",     "Raymond",     "Blackburn",   "Duffy",       "Landry",     "Dougherty",   "Bautista",
    "Shah",        "Potts",       "Arroyo",      "Valentine",   "Meza",       "Gould",       "Vaughan",
    "Fry",         "Rush",        "Avery",       "Herring",     "Dodson",     "Clements",    "Sampson",
    "Tapia",       "Bean",        "Lynn",        "Crane",       "Farley",     "Cisneros",    "Benton",
    "Ashley",      "Mckay",       "Finley",      "Best",        "Blevins",    "Friedman",    "Moses",
    "Sosa",        "Blanchard",   "Huber",       "Frye",        "Krueger",    "Bernard",     "Rosario",
    "Rubio",       "Mullen",      "Benjamin",    "Haley",       "Chung",      "Moyer",       "Choi",
    "Horne",       "Yu",          "Woodward",    "Ali",         "Nixon",      "Hayden",      "Rivers",
    "Estes",       "Mccarty",     "Richmond",    "Stuart",      "Maynard",    "Brandt",      "Oconnell",
    "Hanna",       "Sanford",     "Sheppard",    "Church",      "Burch",      "Levy",        "Rasmussen",
    "Coffey",      "Ponce",       "Faulkner",    "Donaldson",   "Schmitt",    "Novak",       "Costa",
    "Montes",      "Booker",      "Cordova",     "Waller",      "Arellano",   "Maddox",      "Mata",
    "Bonilla",     "Stanton",     "Compton",     "Kaufman",     "Dudley",     "Mcpherson",   "Beltran",
    "Dickson",     "Villegas",    "Proctor",     "Hester",      "Cantrell",   "Daugherty",   "Cherry",
    "Bray",        "Davila",      "Rowland",     "Levine",      "Madden",     "Spence",      "Good",
    "Irwin",       "Werner",      "Krause",      "Petty",       "Whitney",    "Baird",       "Hooper",
    "Pollard",     "Zavala",      "Jarvis",      "Holden",      "Haas",       "Hendrix",     "Mcgrath",
    "Bird",        "Lucero",      "Terrell",     "Riggs",       "Joyce",      "Mercer",      "Rollins",
    "Galloway",    "Duke",        "Odom",        "Andersen",    "Downs",      "Hatfield",    "Benitez",
    "Archer",      "Huerta",      "Travis",      "Mcneil",      "Hinton",     "Zhang",       "Hays",
    "Mayo",        "Fritz",       "Branch",      "Mooney",      "Ewing",      "Ritter",      "Esparza",
    "Frey",        "Braun",       "Riddle",      "Haney",       "Holder",     "Chaney",      "Mcknight",
    "Gamble",      "Vang",        "Cooley",      "Carney",      "Cowan",      "Forbes",      "Ferrell",
    "Davies",      "Barajas",     "Shea",        "Osborn",      "Bright",     "Cuevas",      "Bolton",
    "Murillo",     "Lutz",        "Duarte",      "Kidd",        "Key",        "Cooke",       "Goff",
    "Marin",       "Dotson",      "Cotton",      "Merrill",     "Lindsay",    "Lancaster",   "Mcgowan",
    "Felix",       "Salgado",     "Slater",      "Carver",      "Guthrie",    "Holman",      "Fulton",
    "Snider",      "Sears",       "Witt",        "Newell",      "Byers",      "Lehman",      "Gorman",
    "Costello",    "Donahue",     "Delaney",     "Albert",      "Workman",    "Rosas",       "Springer",
    "Justice",     "Kinney",      "Odell",       "Lake",        "Donnelly",   "Law",         "Dailey",
    "Guevara",     "Shoemaker",   "Barlow",      "Marino",      "Winter",     "Craft",       "Katz",
    "Pickett",     "Espinosa",    "Daly",        "Maloney",     "Goldstein",  "Crowley",     "Vogel",
    "Kuhn",        "Pearce",      "Hartley",     "Cleveland",   "Palacios",   "Mcfadden",    "Britt",
    "Wooten",      "Cortes",      "Dillard",     "Childers",    "Alford",     "Dodd",        "Emerson",
    "Wilder",      "Lange",       "Goldberg",    "Quintero",    "Beach",      "Enriquez",    "Quintana",
    "Helms",       "Mackey",      "Finch",       "Cramer",      "Minor",      "Flanagan",    "Franks",
    "Corona",      "Kendall",     "Mccabe",      "Hendrickson", "Moser",      "Mcdermott",   "Camp",
    "Mcleod",      "Bernal",      "Kaplan",      "Medrano",     "Lugo",       "Tracy",       "Bacon",
    "Crowe",       "Richter",     "Welsh",       "Holley",      "Ratliff",    "Mayfield",    "Talley",
    "Haines",      "Dale",        "Gibbons",     "Hickey",      "Byrne",      "Kirkland",    "Farris",
    "Correa",      "Tillman",     "Sweet",       "Kessler",     "England",    "Hewitt",      "Blanco",
    "Connolly",    "Pate",        "Elder",       "Bruno",       "Holcomb",    "Hyde",        "Mcallister",
    "Cash",        "Christopher", "Whitfield",   "Meeks",       "Hatcher",    "Fink",        "Sutherland",
    "Noel",        "Ritchie",     "Rosa",        "Leal",        "Joyner",     "Starr",       "Morin",
    "Delarosa",    "Connor",      "Hilton",      "Alston",      "Gilliam",    "Wynn",        "Wills",
    "Jaramillo",   "Oneil",       "Nieves",      "Britton",     "Rankin",     "Belcher",     "Guy",
    "Chamberlain", "Tyson",       "Puckett",     "Downing",     "Sharpe",     "Boggs",       "Truong",
    "Pierson",     "Godfrey",     "Mobley",      "John",        "Kern",       "Dye",         "Hollis",
    "Bravo",       "Magana",      "Rutherford",  "Ng",          "Tuttle",     "Lim",         "Romano",
    "Arthur",      "Trejo",       "Knowles",     "Lyon",        "Shirley",    "Quinones",    "Childs",
    "Dolan",       "Head",        "Reyna",       "Saenz",       "Hastings",   "Kenney",      "Cano",
    "Foreman",     "Denton",      "Villalobos",  "Pryor",       "Sargent",    "Doherty",     "Hopper",
    "Phan",        "Womack",      "Lockhart",    "Ventura",     "Dwyer",      "Muller",      "Galindo",
    "Grace",       "Sorensen",    "Courtney",    "Parra",       "Rodrigues",  "Nicholas",    "Ahmed",
    "Mcginnis",    "Langley",     "Madison",     "Locke",       "Jamison",    "Nava",        "Gustafson",
    "Sykes",       "Dempsey",     "Hamm",        "Rodriquez",   "Mcgill",     "Xiong",       "Esquivel",
    "Simms",       "Kendrick",    "Boyce",       "Vigil",       "Downey",     "Mckenna",     "Sierra",
    "Webber",      "Kirkpatrick", "Dickinson",   "Couch",       "Burks",      "Sheehan",     "Pike",
    "Whitley",     "Magee",       "Cheng",       "Sinclair",    "Cassidy",    "Rutledge",    "Burris",
    "Bowling",     "Crabtree",    "Mcnamara",    "Avalos",      "Vu",         "Herron",      "Broussard",
    "Abraham",     "Garland",     "Corbett",     "Corbin",      "Stinson",    "Chin",        "Burt",
    "Hutchins",    "Woodruff",    "Lau",         "Brandon",     "Singer",     "Hatch",       "Rossi",
    "Shafer",      "Ott",         "Goss",        "Gregg",       "Dewitt",     "Tang",        "Polk",
    "Worley",      "Covington",   "Saldana",     "Heller",      "Emery",      "Swartz",      "Cho",
    "Mccray",      "Elmore",      "Rosenberg",   "Simons",      "Clemons",    "Beatty",      "Harden",
    "Herbert",     "Bland",       "Rucker",      "Manley",      "Ziegler",    "Grady",       "Lott",
    "Rouse",       "Gleason",     "Mcclellan",   "Abrams",      "Vo",         "Albright",    "Meier",
    "Dunbar",      "Ackerman",    "Padgett",     "Mayes",       "Tipton",     "Coffman",     "Peralta",
    "Shapiro",     "Roe",         "Weston",      "Plummer",     "Helton",     "Stern",       "Fraser",
    "Stover",      "Fish",        "Schumacher",  "Baca",        "Curran",     "Vinson",      "Vera",
    "Clifton",     "Ervin",       "Eldridge",    "Lowry",       "Childress",  "Becerra",     "Gore",
    "Seymour",     "Chu",         "Field",       "Akers",       "Carrasco",   "Bingham",     "Sterling",
    "Greenwood",   "Leslie",      "Groves",      "Manuel",      "Swain",      "Edmonds",     "Muniz",
    "Thomson",     "Crouch",      "Walden",      "Smart",       "Tomlinson",  "Alfaro",      "Quick",
    "Goldman",     "Mcelroy",     "Yarbrough",   "Funk",        "Hong",       "Portillo",    "Lund",
    "Ngo",         "Elkins",      "Stroud",      "Meredith",    "Battle",     "Mccauley",    "Zapata",
    "Bloom",       "Gee",         "Givens",      "Cardona",     "Schafer",    "Robison",     "Gunter",
    "Griggs",      "Tovar",       "Teague",      "Swift",       "Bowden",     "Schulz",      "Blanton",
    "Buckner",     "Whalen",      "Pritchard",   "Pierre",      "Kang",       "Butts",       "Metcalf",
    "Kurtz",       "Sanderson",   "Tompkins",    "Inman",       "Crowder",    "Dickey",      "Hutchison",
    "Conklin",     "Hoskins",     "Holbrook",    "Horner",      "Neely",      "Tatum",       "Hollingsworth",
    "Draper",      "Clement",     "Lord",        "Reece",       "Feldman",    "Kay",         "Hagen",
    "Crews",       "Bowles",      "Post",        "Jewell",      "Daley",      "Cordero",     "Mckinley",
    "Velasco",     "Masters",     "Driscoll",    "Burrell",     "Valle",      "Crow",        "Devine",
    "Larkin",      "Chappell",    "Pollock",     "Kimball",     "Ly",         "Schmitz",     "Lu",
    "Rubin",       "Self",        "Barrios",     "Pereira",     "Phipps",     "Mcmanus",     "Nance",
    "Steiner",     "Poe",         "Crockett",    "Jeffries",    "Amos",       "Nix",         "Newsome",
    "Dooley",      "Payton",      "Rosen",       "Swenson",     "Connelly",   "Tolbert",     "Segura",
    "Esposito",    "Coker",       "Biggs",       "Hinkle",      "Thurman",    "Drew",        "Ivey",
    "Bullard",     "Baez",        "Neff",        "Maher",       "Stratton",   "Egan",        "Dubois",
    "Gallardo",    "Blue",        "Rainey",      "Yeager",      "Saucedo",    "Ferreira",    "Sprague",
    "Lacy",        "Hurtado",     "Heard",       "Connell",     "Stahl",      "Aldridge",    "Amaya",
    "Forrest",     "Erwin",       "Gunn",        "Swan",        "Butcher",    "Rosado",      "Godwin",
    "Hand",        "Gabriel",     "Otto",        "Whaley",      "Ludwig",     "Clifford",    "Grove",
    "Beaver",      "Silver",      "Dang",        "Hammer",      "Dick",       "Boswell",     "Mead",
    "Colvin",      "Oleary",      "Milligan",    "Goins",       "Ames",       "Dodge",       "Kaur",
    "Escobedo",    "Arredondo",   "Geiger",      "Winkler",     "Dunham",     "Temple",      "Babcock",
    "Billings",    "Grimm",       "Lilly",       "Wesley",      "Mcghee",     "Painter",     "Siegel",
    "Bower",       "Purcell",     "Block",       "Aguilera",    "Norwood",    "Sheridan",    "Cartwright",
    "Coates",      "Davison",     "Regan",       "Ramey",       "Koenig",     "Kraft",       "Bunch",
    "Engel",       "Tan",         "Winn",        "Steward",     "Link",       "Vickers",     "Bragg",
    "Piper",       "Huggins",     "Michel",      "Healy",       "Jacob",      "Mcdonough",   "Wolff",
    "Colbert",     "Zepeda",      "Hoang",       "Dugan",       "Kilgore",    "Meade",       "Guillen",
    "Do",          "Hinojosa",    "Goode",       "Arrington",   "Gary",       "Snell",       "Willard",
    "Renteria",    "Chacon",      "Gallo",       "Hankins",     "Montano",    "Browne",      "Peacock",
    "Ohara",       "Cornell",     "Sherwood",    "Castellanos", "Thorpe",     "Stiles",      "Sadler",
    "Latham",      "Redmond",     "Greenberg",   "Cote",        "Waddell",    "Dukes",       "Diamond",
    "Bui",         "Madrid",      "Alonso",      "Sheets",      "Irvin",      "Hurt",        "Ferris",
    "Sewell",      "Carlton",     "Aragon",      "Blackmon",    "Hadley",     "Hoyt",        "Mcgraw",
    "Pagan",       "Land",        "Tidwell",     "Lovell",      "Miner",      "Doss",        "Dahl",
    "Delatorre",   "Stanford",    "Kauffman",    "Vela",        "Gagnon",     "Winston",     "Gomes",
    "Thacker",     "Coronado",    "Ash",         "Jarrett",     "Hager",      "Samuels",     "Metzger",
    "Raines",      "Spivey",      "Maurer",      "Han",         "Voss",       "Henley",      "Caballero",
    "Caruso",      "Coulter",     "North",       "Finn",        "Cahill",     "Lanier",      "Souza",
    "Mcwilliams",  "Deal",        "Schaffer",    "Urban",       "Houser",     "Cummins",     "Romo",
    "Crocker",     "Bassett",     "Kruse",       "Bolden",      "Ybarra",     "Metz",        "Root",
    "Mcmullen",    "Crump",       "Hagan",       "Guidry",      "Brantley",   "Kearney",     "Beal",
    "Toth",        "Jorgensen",   "Timmons",     "Milton",      "Tripp",      "Hurd",        "Sapp",
    "Whitman",     "Messer",      "Burgos",      "Major",       "Westbrook",  "Castle",      "Serna",
    "Carlisle",    "Varela",      "Cullen",      "Wilhelm",     "Bergeron",   "Burger",      "Posey",
    "Barnhart",    "Hackett",     "Madrigal",    "Eubanks",     "Sizemore",   "Hilliard",    "Hargrove",
    "Boucher",     "Thomason",    "Melvin",      "Roper",       "Barnard",    "Fonseca",     "Pedersen",
    "Quiroz",      "Washburn",    "Holliday",    "Yee",         "Rudolph",    "Bermudez",    "Coyle",
    "Gil",         "Goodrich",    "Pina",        "Elias",       "Lockwood",   "Cabral",      "Carranza",
    "Duvall",      "Cornelius",   "Mccollum",    "Street",      "Mcneal",     "Connors",     "Angel",
    "Paulson",     "Hinson",      "Keenan",      "Sheldon",     "Farr",       "Eddy",        "Samuel",
    "Ledbetter",   "Ring",        "Betts",       "Fontenot",    "Gifford",    "Hannah",      "Hanley",
    "Person",      "Fountain",    "Levin",       "Stubbs",      "Hightower",  "Murdock",     "Koehler",
    "Ma",          "Engle",       "Smiley",      "Carmichael",  "Sheffield",  "Langston",    "Mccracken",
    "Yost",        "Trotter",     "Story",       "Starks",      "Lujan",      "Blount",      "Cody",
    "Rushing",     "Benoit",      "Herndon",     "Jacobsen",    "Nieto",      "Wiseman",     "Layton",
    "Epps",        "Shipley",     "Leyva",       "Reeder",      "Brand",      "Roland",      "Fitch",
    "Rico",        "Napier",      "Cronin",      "Mcqueen",     "Paredes",    "Trent",       "Christiansen",
    "Pettit",      "Spangler",    "Langford",    "Benavides",   "Penn",       "Paige",       "Weir",
    "Dietz",       "Prater",      "Brewster",    "Louis",       "Diehl",      "Pack",        "Spaulding",
    "Aviles",      "Ernst",       "Nowak",       "Olvera",      "Rock",       "Mansfield",   "Aquino",
    "Ogden",       "Stacy",       "Rizzo",       "Sylvester",   "Gillis",     "Sands",       "Machado",
    "Lovett",      "Duong",       "Hyatt",       "Landis",      "Platt",      "Bustamante",  "Hedrick",
    "Pritchett",   "Gaston",      "Dobson",      "Caudill",     "Tackett",    "Bateman",     "Landers",
    "Carmona",     "Gipson",      "Uribe",       "Mcneill",     "Ledford",    "Mims",        "Abel",
    "Gold",        "Smallwood",   "Thorne",      "Mchugh",      "Dickens",    "Leung",       "Tobin",
    "Kowalski",    "Medeiros",    "Cope",        "Kraus",       "Quezada",    "Overton",     "Montalvo",
    "Staley",      "Woody",       "Hathaway",    "Osorio",      "Laird",      "Dobbs",       "Capps",
    "Putnam",      "Lay",         "Francisco",   "Adair",       "Bernstein",  "Hutton",      "Burkett",
    "Rhoades",     "Richey",      "Yanez",       "Bledsoe",     "Mccain",     "Beyer",       "Cates",
    "Roche",       "Spicer",      "Queen",       "Doty",        "Darling",    "Darby",       "Sumner",
    "Kincaid",     "Hay",         "Grossman",    "Lacey",       "Wilkes",     "Humphries",   "Paz",
    "Darnell",     "Keys",        "Kyle",        "Lackey",      "Vogt",       "Locklear",    "Kiser",
    "Presley",     "Bryson",      "Bergman",     "Peoples",     "Fair",       "Mcclendon",   "Corley",
    "Prado",       "Christie",    "Delong",      "Skaggs",      "Dill",       "Shearer",     "Judd",
    "Stapleton",   "Flaherty",    "Casillas",    "Pinto",       "Haywood",    "Youngblood",  "Toney",
    "Ricks",       "Granados",    "Crum",        "Triplett",    "Soriano",    "Waite",       "Hoff",
    "Anaya",       "Crenshaw",    "Jung",        "Canales",     "Cagle",      "Denny",       "Marcus",
    "Berman",      "Munson",      "Ocampo",      "Bauman",      "Corcoran",   "Keen",        "Zimmer",
    "Friend",      "Ornelas",     "Varner",      "Pelletier",   "Vernon",     "Blum",        "Albrecht",
    "Culver",      "Schuster",    "Cuellar",     "Mccord",      "Shultz",     "Mcrae",       "Moreland",
    "Calvert",     "William",     "Whittington", "Eckert",      "Keene",      "Mohr",        "Hanks",
    "Kimble",      "Cavanaugh",   "Crowell",     "Russ",        "Feliciano",  "Crain",       "Busch",
    "Mccormack",   "Drummond",    "Omalley",     "Aldrich",     "Luke",       "Greco",       "Mott",
    "Oakes",       "Mallory",     "Mclain",      "Burrows",     "Otero",      "Allred",      "Eason",
    "Finney",      "Weller",      "Waldron",     "Champion",    "Jeffers",    "Coon",        "Rosenthal",
    "Huddleston",  "Solano",      "Hirsch",      "Akins",       "Olivares",   "Song",        "Sneed",
    "Benedict",    "Bain",        "Okeefe",      "Hidalgo",     "Matos",      "Stallings",   "Paris",
    "Gamez",       "Kenny",       "Quigley",     "Marrero",     "Fagan",      "Dutton",      "Atwood",
    "Pappas",      "Bagley",      "Mcgovern",    "Lunsford",    "Moseley",    "Read",        "Oakley",
    "Ashby",       "Granger",     "Shaver",      "Hope",        "Coe",        "Burroughs",   "Helm",
    "Ambrose",     "Neumann",     "Michaels",    "Prescott",    "Light",      "Dumas",       "Flood",
    "Stringer",    "Currie"};

  auto firstName = names[rand() % sizeof(names) / sizeof(names[0])];         // NOLINT
  auto surName = surnames[rand() % sizeof(surnames) / sizeof(surnames[0])];  // NOLINT

  std::string fullName = firstName;
  fullName.append(surName);
  fullName[0] = static_cast<char>(std::tolower(fullName[0]));  // NOLINT(misc-include-cleaner)

  return std::make_tuple(firstName, surName, fullName);
}

}  // namespace school
