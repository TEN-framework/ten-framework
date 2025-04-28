///////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2018 - 2022.                 //
//  Distributed under the Boost Software License,                //
//  Version 1.0. (See accompanying file LICENSE_1_0.txt          //
//  or copy at http://www.boost.org/LICENSE_1_0.txt)             //
///////////////////////////////////////////////////////////////////

// This Miller-Rabin primality test is loosely based on
// an adaptation of some code from Boost.Multiprecision.
// The Boost.Multiprecision code can be found here:
// https://www.boost.org/doc/libs/1_73_0/libs/multiprecision/doc/html/boost_multiprecision/tut/primetest.html

#include <chrono>
#include <random>

#include <examples/example_uintwide_t.h>
#include <math/wide_integer/uintwide_t.h>

#if defined(__clang__)
  #if defined __has_feature && __has_feature(thread_sanitizer)
  #define UINTWIDE_T_REDUCE_TEST_DEPTH
  #endif
#elif defined(__GNUC__)
  #if defined(__SANITIZE_THREAD__) || defined(WIDE_INTEGER_HAS_COVERAGE)
  #define UINTWIDE_T_REDUCE_TEST_DEPTH
  #endif
#elif defined(_MSC_VER)
  #if defined(_DEBUG)
  #define UINTWIDE_T_REDUCE_TEST_DEPTH
  #endif
#endif

namespace local_example008_miller_rabin_prime
{
  #if defined(WIDE_INTEGER_NAMESPACE)
  using wide_integer_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(512))>;
  using distribution_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uniform_int_distribution<wide_integer_type::my_width2, typename wide_integer_type::limb_type>;
  #else
  using wide_integer_type = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(512))>;
  using distribution_type = ::math::wide_integer::uniform_int_distribution<wide_integer_type::my_width2, typename wide_integer_type::limb_type>;
  #endif

  using random_engine1_type = std::linear_congruential_engine<std::uint32_t, UINT32_C(48271), UINT32_C(0), UINT32_C(2147483647)>;
  using random_engine2_type = std::mt19937;

  template<typename IntegralTimePointType,
           typename ClockType = std::chrono::high_resolution_clock>
  auto time_point() -> IntegralTimePointType
  {
    using local_integral_time_point_type = IntegralTimePointType;
    using local_clock_type               = ClockType;

    const auto current_now =
      static_cast<std::uintmax_t>
      (
        std::chrono::duration_cast<std::chrono::nanoseconds>
        (
          local_clock_type::now().time_since_epoch()
        ).count()
      );

    return static_cast<local_integral_time_point_type>(current_now);
  }

  auto example008_miller_rabin_prime_run() -> bool
  {
    // Use a pseudo-random seed for this test.

    random_engine1_type generator1(time_point<typename random_engine1_type::result_type>());
    random_engine2_type generator2(time_point<typename random_engine2_type::result_type>());

    distribution_type distribution1;
    distribution_type distribution2;

    wide_integer_type p0;
    wide_integer_type p1;

    for(;;)
    {
      p0 = distribution1(generator1);

      const bool miller_rabin_result = miller_rabin(p0,
                                                    25U,
                                                    distribution2,
                                                    generator2);

      if(miller_rabin_result)
      {
        break;
      }
    }

    for(;;)
    {
      p1 = distribution1(generator1);

      const bool miller_rabin_result = miller_rabin(p1,
                                                    25U,
                                                    distribution2,
                                                    generator2);

      if(miller_rabin_result)
      {
        break;
      }
    }

    const auto gd = gcd(p0, p1);

    const auto result_is_ok = (   (p0 != 0U)
                               && (p1 != 0U)
                               && (p0 != p1)
                               && (gd == 1U));

    return result_is_ok;
  }

  auto example008_miller_rabin_prime_check_known_primes() -> bool
  {
    #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
    constexpr auto primes_array_count = static_cast<std::size_t>(UINT8_C(28));
    #else
    constexpr auto primes_array_count = static_cast<std::size_t>(UINT8_C(4));
    #endif

    using known_primes_array_type = std::array<wide_integer_type, primes_array_count>;

    const auto known_primes =
      known_primes_array_type
      {
        wide_integer_type("1350668399695862095705378871871078275422204299410029680022807229578447177486838048356600483814274341298941322922278646278082691661579262242972653147843881"),
        wide_integer_type("6316533715112802448288092604478054524383926634811945757057640178392735989589363869889681632218704486270766492727631128625563552112105808239098327091824521"),
        wide_integer_type("4858586217250992689079636758246308353014067931891277522662097117263017348927716654355835947116278934662184099633799310227887262043463022049904058963602113"),
        wide_integer_type("6019419207947545798517327619748393618031791796951797411174747320842950599875485354843571998416270218797959087329922954007679373990444302239974053043273173"),
        #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
        wide_integer_type("2623115045263242825415413671896042727657332322988674224271913776550889477454553016178535650310812678634291673863620916079255756983038819930429300055323617"),
        wide_integer_type("3003848015480415325081996052076028272879191508991266945638241025854760400469061295504211346716924852976586414875652009123799821021749062743721323009536589"),
        wide_integer_type("5568337324913975213045841890066332347573591111951280366169746765603974031759890673193278087261510160253710813073898966574448941498158599660295718450413969"),
        wide_integer_type("3899092769157768302620084735536775086413273751852623051586317377248088862913234601759046503051512549835620467902724675884204543889535019159859544426497689"),
        wide_integer_type("4880233062753644655310200777146080067274805955812121495803730551248986874932581198974746359764046223936780348192151675074968829396304926645738459150294633"),
        wide_integer_type("2981493811876081198746361294505357003171188931549722884629126574527379655453586512421136559407326464729329418005336291630284117366942572742648076658621793"),
        wide_integer_type("2311447178623403864122136445969808248907814278870229050269813814615527678820577823271523594807805953636283537824863549493053563901315483672394027660220781"),
        wide_integer_type("3479071810465946698893564410603359501484777617936440665021184921238051587645202575681993903459105745702439079512468629916692256642323236436208584510861069"),
        wide_integer_type("6118926406334289174567277866138349610455081780960756500741587206842789898877382284319927664574788247571122480984323699664304605053422838920108157647074869"),
        wide_integer_type("5590335646861485447121090043941130683713858202654698267088158519958402198517001330817356690577780346497360906009275424722046395721643723271668335885539113"),
        wide_integer_type("4600898139597524599763199946635257244387697856987579864564123646981189843032360401339979289535161473255665149405050376631425564431369445938758414793076901"),
        wide_integer_type("4315306784386454402272613378470925299536719882405387670534486076073677209483117906338062419164521262724975133161632270609178741350949903249729845258787733"),
        wide_integer_type("5950481506592244740042852475032761031143767332677168744969800384139374680573074295152251652710948027287864530732080292167390630350615454944427411014041141"),
        wide_integer_type("2440980104093377665340267319609708156057114020162041419246265162721394309112466086681318371709997847741488133816937847915825174882442455843460458950869789"),
        wide_integer_type("4365212508738551992055010318700772806707699100481394233862331936291205769739000493695684304355128758915801551136191115096132710582148081523015262320554393"),
        wide_integer_type("3781985616534097915844902700666149069763815834328308181456271045363812894911075580338484224113219563989181987256586477816830560104618708627118533065620341"),
        wide_integer_type("6312111327629281870461906157964428032414795872246359612186802568161839869980963702584858942079006607549034530874234327750267701726691921864700120963919449"),
        wide_integer_type("3457298715359430348699008011012486630342527178370025440230151409769901896560575367519479882530184023906479055022991404575620008255488477625490456126386873"),
        wide_integer_type("6382461542071546162407085982360686365702447907013055208214815317050235935992759617609828663308694238400452867139555655406217610647086518464256672670959569"),
        wide_integer_type("6068775473164015507047528585305834886132568156674171703370121517708258623288855684363295365245992564330637765075823775316688120061787431995297819368086373"),
        wide_integer_type("6070463708262743334734874588409415398342276766482324769914482379758057124149747442871974246654319915559096090055433661153568555118115638391055646124587309"),
        wide_integer_type("6297032304320012154880388332146509431687962719889593388307760834122241251586999352542604072606959905346551948057937113980815914420810768937615250771878461"),
        wide_integer_type("6128722501467469312821769346772700466471416838690252468887811095595451174847178063191879577266340073924505285489011360630545669076007556359186484284685741"),
        wide_integer_type("5334118887555315545538097460404232946047447099403008196978701351362864400468260258361331949883359663365575253347639015242447351434717477630139194429944273")
        #endif
      };

    random_engine1_type generator(time_point<typename random_engine1_type::result_type>());

    distribution_type distribution;

    auto result_is_ok = true;

    for(const auto& p : known_primes)
    {
      const auto result_known_prime_is_ok = miller_rabin(p,
                                                         25U,
                                                         distribution,
                                                         generator);

      result_is_ok = (result_known_prime_is_ok && result_is_ok);
    }

    return result_is_ok;
  }
} // namespace local_example008_miller_rabin_prime

#if defined(WIDE_INTEGER_NAMESPACE)
auto WIDE_INTEGER_NAMESPACE::math::wide_integer::example008_miller_rabin_prime() -> bool
#else
auto ::math::wide_integer::example008_miller_rabin_prime() -> bool
#endif
{
  auto result_is_ok = true;

  for(auto   i = static_cast<unsigned>(UINT8_C(0));
  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
             i < static_cast<unsigned>(UINT8_C(8));
  #else
             i < static_cast<unsigned>(UINT8_C(1));
  #endif
           ++i)
  {
    const auto result_prime_run_is_ok =
      local_example008_miller_rabin_prime::example008_miller_rabin_prime_run();
  
    result_is_ok = (result_prime_run_is_ok && result_is_ok);
  }

  {
    const auto result_known_primes_is_ok =
      local_example008_miller_rabin_prime::example008_miller_rabin_prime_check_known_primes();

    result_is_ok = (result_known_primes_is_ok && result_is_ok);
  }

  return result_is_ok;
}

// Enable this if you would like to activate this main() as a standalone example.
#if defined(WIDE_INTEGER_STANDALONE_EXAMPLE008_MILLER_RABIN_PRIME)

#include <iomanip>
#include <iostream>

auto main() -> int
{
  #if defined(WIDE_INTEGER_NAMESPACE)
  const auto result_is_ok = WIDE_INTEGER_NAMESPACE::math::wide_integer::example008_miller_rabin_prime();
  #else
  const auto result_is_ok = ::math::wide_integer::example008_miller_rabin_prime();
  #endif

  std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;

  return (result_is_ok ? 0 : -1);
}

#endif
