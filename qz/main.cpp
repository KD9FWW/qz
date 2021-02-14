// R. W. Helgeson
// quiz v4
// 2021 January 10
//       1         2         3         4         5         6         7         8
//345678901234567890123456789012345678901234567890123456789012345678901234567890
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <random>
#include <regex>
#include <set>
namespace Rand {
   class RandomEngine: private std::default_random_engine {
   public:
      using std::default_random_engine::result_type;
      using std::default_random_engine::min;
      using std::default_random_engine::max;
      using std::default_random_engine::operator();
      explicit RandomEngine( bool autoSeed = true ) {
         if( autoSeed ) { RandomEngine::seed(); }
      }
      void seed( result_type seedValue = 0 ) {
         volatile auto tick{ std::time( nullptr ) };
         if( !seedValue )
            seedValue = static_cast<result_type>(0x7FFF'7FFFul & tick);
         std::default_random_engine::seed( seedValue );
      }
   };
   class CoinToss {
      RandomEngine e;
      std::bernoulli_distribution d;
   public:
      CoinToss( bool autoSeed = true ) : d{} { if( autoSeed ) e.seed(); }
      const bool operator()() { return d( e ); }
   };
}
namespace Qweez {
   using N = size_t;
   const N CARDINAL{ 4 };  // number of answers in question body
   using CCP = const char *;
   CCP configFileDefault{ "/.quiz" };
   using STRING = std::string;
   using TAG = STRING;
   using TAGS = std::vector<TAG>;
   using QUEUE = std::queue<TAG>;
   using RATING = std::map<TAG,signed>;
   using TEXT = STRING;
   using SEQ = std::vector<N>;
   using BODY = std::vector<TEXT>;
   class Seq : private std::vector< SEQ > {
      bool autoSeed;
      SEQ canon;
      N iNdex;
      Rand::RandomEngine engine;
      auto shfl() {
         if( autoSeed ) { engine.seed(); }
         std::shuffle(this->begin(), this->end(), engine);
      }
   public:
      const auto operator()( const BODY &arg, N n ) const {
         return arg.at( this->at(iNdex).at(n) );
      }
      auto operator++() {
         ++iNdex;
         if( this->size() <= iNdex ) {
            shfl();
            iNdex = 0;
         }
      }
      explicit Seq( const N &cardinal = CARDINAL, bool seed = true )
      : autoSeed{ seed }, canon{} {
         for( auto ordinal{ 0 }; ordinal < cardinal; ++ordinal ) {
            canon.push_back( ordinal );
         }
         auto seq{ canon };
         this->push_back( seq );
         while ( std::next_permutation( seq.begin(), seq.end() ) ) {
            this->push_back( seq );
         }
         iNdex = this->size();   // this->size() = factorial(n);
         this->operator++();
      }
   };
   class Body : public BODY {
      TEXT text;
      N iNdex;
   public:
      friend class Items;
      explicit Body() : text{}, iNdex{0} {}
      const auto operator()() const { return this->at( iNdex ); }
      const auto operator()( N n ) const { return this->at( n ); }
      const auto operator()( const TEXT &t ) const {
         return ( this->at( iNdex ) == t );
      }
      friend auto & operator<<( std::ostream &os, const Body &b ) {
         return os << b.text.c_str();
      }
   };
   using ITEMS = std::map<TAG,Body>;
   class Items : private ITEMS {
   public:
      using ITEMS::at;
      using ITEMS::begin;
      using ITEMS::end;
      using ITEMS::size;
      explicit Items( const STRING &fn, const STRING &re = R"(\d-\w+)" ) {
         std::ifstream ifs{ fn };
         if( !ifs.good() ) throw;
         if( re.empty() ) { throw; }
         const std::regex regEx{ re };
         for( TEXT text; ifs >> text; ) {
            if( std::regex_match( text, regEx ) ) {
               TAG tag{ text };
               ifs >> text >> std::ws;
               Body body;
               body.iNdex = static_cast<N>(std::toupper(*text.begin() ) - 'A');
               std::getline( ifs, body.text );
               for( auto n{ 0 }; n < CARDINAL; ++n ) {
                  ifs >> text >> std::ws;  // discard label, eg.: A.
                  std::getline( ifs, text );
                  body.push_back( text );
               }
               this->operator[]( tag ) = body;
            }
         }
         ifs.close();
      }
   };
   class Quiz;
   class Score {
      STRING::value_type prior; // if answered, 'twas a dinger or a buzzer
      N total, plus, minus;
      auto increment() { ++total; ++plus; prior = '+'; }
      auto decrement() { ++total; ++minus; prior = '-'; }
   public:
      friend Quiz;
      explicit Score() : prior{ 0 }, total{ 0 }, plus{ 0 }, minus{ 0 } { }
      auto operator ++() { return increment(); }
      auto operator --() { return decrement(); }
      friend auto & operator<<( std::ostream &os, const Score &s ) {
         //         os << s.total << ' ';
         os << '+' << s.plus << ' ';
         os << '-' << s.minus;
         if( s.prior ) os << ' ' << s.prior;
         return os << '\n';
      }
   };
   class SymbolTable : public std::map<STRING,STRING> {
   public:
      explicit SymbolTable( int ac, CCP av[], CCP ev[] )  {
         char n{ '0' };
         STRING dollar{ '$' };
         for( auto iter{ av }; *iter; ++iter )
         this->operator[]( dollar + n++ ) = *iter;
         for( auto iter{ ev }; *iter; ++iter ) {
            STRING s{ *iter };
            auto found = s.find( '=' );
            this->operator[]( s.substr( 0, found ) ) = s.substr( found + 1 );
         }
      }
   };
   class Quiz {
      SymbolTable syms;
      Items items;
      Score score;
      TAGS todo;
      QUEUE redo;
      RATING rating;
      Rand::RandomEngine engine;
      Seq scrambler;
      struct {
         std::istream &in;
         std::ostream &out;
         std::ostream &err;
      } userIO;
      STRING fName;
      const auto empty() { return redo.empty() && todo.empty(); }
      const auto human( const TAG &tag ) {
         Body body{ items.at( tag ) };
         userIO.out << body << '\n';   // body text (the item's question text)
         char c{ 'A' };
         // present body according in scramble order
         for( auto n{ 0 }; n < CARDINAL; ++n ) {
            userIO.out << c++ << ". " << scrambler(body, n) << '\n';
         }
         TEXT text;
         bool proceed{ false };
         while( !proceed ) {
            userIO.in >> text;
            if( userIO.in.eof() ) { return false; }  // EOT from human
            auto cmd{ std::toupper( *text.begin()) };
            if( 'Q' == cmd ) { return  false ;}
            auto n{ static_cast<N>( cmd - 'A' ) };
            try { // rely on container's .at() to throw()
               text.clear();
               text = scrambler(body, n);
               proceed = true;
            } catch ( ... ) { userIO.err << "\r?\r"; continue; }
            if( body( text ) ) { ++(rating.at( tag )); } // dinger: big wiener.
            else {
               --(rating.at( tag ));               // buzzer: wrongo, bucko.
               redo.push( tag );                   // queue for later
               userIO.out << '\n' << body() << '\n'; // instant re-inforcement
            }
// rwh 2021Feb14
            ++scrambler;               // advance to next random sequence
// effect a post increment.
// if more operator()s are needed in the future,
// like post incr, so that: scrambler(body,n)++ works
// implement the class using iterators
// end 2021Feb14
         }
         return true;
      }
   public:
      explicit Quiz( int c, CCP a[], CCP e[] )
      : syms{ c, a, e }, items{ syms.at( "$1" ) }, score{}, todo{}, redo{},
      rating{}, engine{}, scrambler{ CARDINAL },
      userIO{ std::cin, std::cout, std::cerr }, fName{} {
         try { fName = syms.at( "$2" ); }
         catch ( ... ) { fName = syms.at( "HOME" ) + configFileDefault; }
         std::ifstream is{ fName };
         if( is.bad() ) throw;
         TAG tag;
         std::set<TAG> exempt;
         while ( is >> tag ) {
            exempt.insert( tag );
         }
         is.close();
         for( auto iter : items ) {
            tag = iter.first;
            if( exempt.find( tag ) == exempt.end() ) {
               todo.push_back( tag );
               rating[ tag ] = 0;
            }
            else {
               exempt.erase( tag );
            }
         }
         score.plus = items.size() - todo.size();
         engine.seed();
         std::shuffle(todo.begin(), todo.end(), engine);
      }
      auto operator()() {
         Rand::CoinToss coin;
         for( TAG tag; !empty(); ) {
            tag.clear();
            if( !redo.empty() ) {
               if( coin() ) {
                  tag = redo.front();
                  redo.pop();
               }
            }
            if( tag.empty() ) {
               tag = todo.back();
               todo.pop_back();
            }
            userIO.out << '\n' << tag << ' ' << todo.size() << ' ';
            userIO.out << score;
            if( !human( tag ) ) break;    // EOT sent
            if( rating.at( tag ) < 0 ) {
               --score;
            }
            else {
               ++score;
               std::ofstream of{ fName, std::ios_base::app };
               of << tag << '\n';
               of.close();
            }
         }
         return empty();
      }
   };
}
int main( int ac, const char *av[], const char *ev[] ) {
   Qweez::Quiz quiz{ ac, av, ev };
   return quiz();
}
//345678901234567890123456789012345678901234567890123456789012345678901234567890
//       1         2         3         4         5         6         7         8
