///////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2021 - 2022.                 //
//  Distributed under the Boost Software License,                //
//  Version 1.0. (See accompanying file LICENSE_1_0.txt          //
//  or copy at http://www.boost.org/LICENSE_1_0.txt)             //
///////////////////////////////////////////////////////////////////

#include <random>
#include <string>

#include <examples/example_uintwide_t.h>
#include <math/wide_integer/uintwide_t.h>

namespace local_rsa
{
  template<const std::size_t RsaBitCount,
           typename LimbType = std::uint32_t,
           typename AllocatorType = std::allocator<void>>
  class rsa_base
  {
  public:
    static constexpr std::size_t bit_count = RsaBitCount;

    using allocator_type = typename std::allocator_traits<AllocatorType>::template rebind_alloc<LimbType>;

    #if defined(WIDE_INTEGER_NAMESPACE)
    using my_uintwide_t  = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(bit_count),
                                                                                  LimbType,
                                                                                  allocator_type>;
    #else
    using my_uintwide_t  = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(bit_count),
                                                          LimbType,
                                                          allocator_type>;
    #endif

    using limb_type      = typename my_uintwide_t::limb_type;

    using crypto_char    = my_uintwide_t;
    using crypto_alloc   = typename std::allocator_traits<allocator_type>::template rebind_alloc<crypto_char>;
    #if defined(WIDE_INTEGER_NAMESPACE)
    using crypto_string  = WIDE_INTEGER_NAMESPACE::math::wide_integer::detail::dynamic_array<crypto_char, crypto_alloc>;
    #else
    using crypto_string  = ::math::wide_integer::detail::dynamic_array<crypto_char, crypto_alloc>;
    #endif

    using private_key_type =
      struct
      {
        my_uintwide_t s;
        my_uintwide_t p;
        my_uintwide_t q;
      };

    using public_key_type =
      struct public_key_type
      {
        my_uintwide_t r;
        my_uintwide_t m;
      };

    virtual ~rsa_base() = default;

    struct euclidean
    {
      template<typename IntegerType>
      static auto extended_euclidean(const IntegerType& a, // NOLINT(misc-no-recursion)
                                     const IntegerType& b,
                                           IntegerType* x, // NOLINT(bugprone-easily-swappable-parameters)
                                           IntegerType* y) -> IntegerType
      {
        // Recursive extended Euclidean algorithm.
        using local_integer_type = IntegerType;

        if(a == 0)
        {
          *x = local_integer_type { 0U };
          *y = local_integer_type { 1U };

          return b;
        }

        local_integer_type tmp_x;
        local_integer_type tmp_y;

        local_integer_type gcd_ext = extended_euclidean(b % a, a, &tmp_x, &tmp_y);

        *x = tmp_y - ((b / a) * tmp_x);
        *y = tmp_x;

        return gcd_ext;
      }
    };

    class encryptor
    {
    public:
      explicit encryptor(const public_key_type& key) : public_key(key) { }

      template<typename InputIterator,
               typename OutputIterator>
      auto encrypt(InputIterator in_first, const std::size_t count, OutputIterator out) -> void
      {
        for(auto it = in_first; it != in_first + static_cast<typename std::iterator_traits<InputIterator>::difference_type>(count); ++it) // NOLINT(altera-id-dependent-backward-branch)
        {
          *out++ = powm(my_uintwide_t(*it), public_key.r, public_key.m);
        }
      }

    private:
      const public_key_type& public_key; // NOLINT(readability-identifier-naming)
    };

    class decryptor
    {
    public:
      explicit decryptor(const private_key_type& key) : private_key(key) { }

      template<typename InputIterator,
               typename OutputIterator>
      auto decrypt(InputIterator cry_in, const std::size_t count, OutputIterator cypher_out) -> void
      {
        InputIterator cry_end(cry_in + static_cast<typename std::iterator_traits<InputIterator>::difference_type>(count));

        for(auto it = cry_in; it !=  cry_end; ++it) // NOLINT(altera-id-dependent-backward-branch)
        {
          const my_uintwide_t tmp = powm(*it, private_key.s, private_key.q * private_key.p);

          *cypher_out++ = static_cast<typename std::iterator_traits<OutputIterator>::value_type>(static_cast<limb_type>(tmp));
        }
      }

    private:
      const private_key_type& private_key; // NOLINT(readability-identifier-naming)
    };

    rsa_base(const rsa_base& other) : my_p       (other.my_p),
                                      my_q       (other.my_q),
                                      my_r       (other.my_r),
                                      my_m       (other.my_m),
                                      phi_of_m   (other.phi_of_m),
                                      public_key (other.public_key),
                                      private_key(other.private_key) { }

    rsa_base(rsa_base&& other) noexcept : my_p       (static_cast<my_uintwide_t&&   >(other.my_p)),
                                          my_q       (static_cast<my_uintwide_t&&   >(other.my_q)),
                                          my_r       (static_cast<my_uintwide_t&&   >(other.my_r)),
                                          my_m       (static_cast<my_uintwide_t&&   >(other.my_m)),
                                          phi_of_m   (static_cast<my_uintwide_t&&   >(other.phi_of_m)),
                                          public_key (static_cast<public_key_type&& >(other.public_key)),
                                          private_key(static_cast<private_key_type&&>(other.private_key)) { }

    auto operator=(const rsa_base& other) -> rsa_base&
    {
      if(this != &other)
      {
        my_p        = other.my_p;
        my_q        = other.my_q;
        my_r        = other.my_r;
        my_m        = other.my_m;
        phi_of_m    = other.phi_of_m;
        public_key  = other.public_key;
        private_key = other.private_key;
      }

      return *this;
    }

    auto operator=(rsa_base&& other) noexcept -> rsa_base&
    {
      my_p        = static_cast<my_uintwide_t&&   >(other.my_p);
      my_q        = static_cast<my_uintwide_t&&   >(other.my_q);
      my_r        = static_cast<my_uintwide_t&&   >(other.my_r);
      my_m        = static_cast<my_uintwide_t&&   >(other.my_m);
      phi_of_m    = static_cast<my_uintwide_t&&   >(other.phi_of_m);
      public_key  = static_cast<public_key_type&& >(other.public_key);
      private_key = static_cast<private_key_type&&>(other.private_key);

      return *this;
    }

    auto getPublicKey () const -> const public_key_type&  { return public_key; }  // NOLINT(readability-identifier-naming)
    auto getPrivateKey() const -> const private_key_type& { return private_key; } // NOLINT(readability-identifier-naming)

    auto get_p() const -> const crypto_char& { return getPrivateKey().p; }
    auto get_q() const -> const crypto_char& { return getPrivateKey().q; }
    auto get_d() const -> const crypto_char& { return getPrivateKey().s; }
    auto get_n() const -> const crypto_char& { return getPublicKey().m; }

    auto encrypt(const std::string& str) const -> crypto_string
    {
      crypto_string str_out(str.length());

      encryptor(public_key).encrypt(str.cbegin(), str.length(), str_out.begin());

      return str_out;
    } // LCOV_EXCL_LINE

    auto decrypt(const crypto_string& str) const -> std::string
    {
      std::string res(str.size(), '\0');

      decryptor(private_key).decrypt(str.cbegin(), str.size(), res.begin());

      return res;
    }

    template<typename RandomEngineType = std::minstd_rand>
    static auto is_prime(const my_uintwide_t& p,
                         const RandomEngineType& generator = RandomEngineType(static_cast<typename RandomEngineType::result_type>(std::clock()))) -> bool
    {
      #if defined(WIDE_INTEGER_NAMESPACE)
      using local_distribution_type =
        WIDE_INTEGER_NAMESPACE::math::wide_integer::uniform_int_distribution<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t(bit_count), limb_type, allocator_type>;
      #else
      using local_distribution_type =
        ::math::wide_integer::uniform_int_distribution<static_cast<math::wide_integer::size_t>(bit_count), limb_type, allocator_type>;
      #endif

      local_distribution_type distribution;

      RandomEngineType local_generator(generator);

      const bool miller_rabin_result_is_ok = miller_rabin(p, 25U, distribution, local_generator);

      return miller_rabin_result_is_ok;
    }

    rsa_base() = delete;

  protected:
    my_uintwide_t    my_p;            // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes,readability-identifier-naming)
    my_uintwide_t    my_q;            // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes,readability-identifier-naming)
    my_uintwide_t    my_r;            // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes,readability-identifier-naming)
    my_uintwide_t    my_m;            // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes,readability-identifier-naming)
    my_uintwide_t    phi_of_m    { }; // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes,readability-identifier-naming)
    public_key_type  public_key  { }; // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes,readability-identifier-naming)
    private_key_type private_key { }; // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes,readability-identifier-naming)

    rsa_base(my_uintwide_t p_in,
             my_uintwide_t q_in,
             my_uintwide_t r_in) : my_p(std::move(p_in)),
                                   my_q(std::move(q_in)),
                                   my_r(std::move(r_in)),
                                   my_m(my_p * my_q)
    {
      public_key = public_key_type { my_r, my_m }; // NOLINT(cppcoreguidelines-prefer-member-initializer)
    }

    auto calculate_private_key() -> void
    {
      my_uintwide_t a = phi_of_m;
      my_uintwide_t b = my_r;

      my_uintwide_t x { };
      my_uintwide_t s { };

      euclidean::extended_euclidean(a, b, &x, &s);

      s = is_neg(s) ? make_positive(s, phi_of_m) : s;

      private_key = private_key_type { s, my_p, my_q };
    }

  private:
    static auto is_neg(const my_uintwide_t& x) -> bool
    {
      const bool x_is_neg = ((x & (my_uintwide_t(1U) << (std::numeric_limits<my_uintwide_t>::digits - 1))) != 0);

      return x_is_neg;
    }

    static auto make_positive(const my_uintwide_t& number, const my_uintwide_t& modulus) -> my_uintwide_t // NOLINT(bugprone-easily-swappable-parameters)
    {
      my_uintwide_t tmp = number;

      while(is_neg(tmp)) // NOLINT(altera-id-dependent-backward-branch)
      {
        tmp += modulus;
      }

      return tmp;
    }
  };

  template<const std::size_t RsaBitCount,
           typename LimbType = std::uint32_t>
  class rsa_fips : public rsa_base<RsaBitCount, LimbType>
  {
  private:
    using base_class_type = rsa_base<RsaBitCount, LimbType>;

  public:
    rsa_fips(const typename base_class_type::my_uintwide_t& p_in,
             const typename base_class_type::my_uintwide_t& q_in,
             const typename base_class_type::my_uintwide_t& r_in)
      : base_class_type(p_in, q_in, r_in)
    {
      const typename base_class_type::my_uintwide_t my_one(1U);

      base_class_type::phi_of_m = lcm(base_class_type::my_p - my_one,
                                      base_class_type::my_q - my_one);

      base_class_type::calculate_private_key();
    }

    rsa_fips(const rsa_fips& other) : base_class_type(other) { }

    rsa_fips(rsa_fips&& other) noexcept : base_class_type(other) { }

    ~rsa_fips() override = default;

    auto operator=(const rsa_fips& other) -> rsa_fips& // NOLINT(cert-oop54-cpp)
    {
      static_cast<void>(base_class_type::operator=(other));

      return *this;
    }

    auto operator=(rsa_fips&& other) noexcept -> rsa_fips&
    {
      static_cast<void>(base_class_type::operator=(other));

      return *this;
    }
  };

  template<const std::size_t RsaBitCount,
           typename LimbType = std::uint32_t>
  class rsa_traditional : public rsa_base<RsaBitCount, LimbType>
  {
  private:
    using base_class_type = rsa_base<RsaBitCount, LimbType>;

  public:
    rsa_traditional(const typename base_class_type::my_uintwide_t& p_in,
                    const typename base_class_type::my_uintwide_t& q_in,
                    const typename base_class_type::my_uintwide_t& r_in)
      : base_class_type(p_in, q_in, r_in)
    {
      const typename base_class_type::my_uintwide_t my_one(1U);

      base_class_type::phi_of_m = (  (base_class_type::my_p - my_one)
                                   * (base_class_type::my_q - my_one));

      base_class_type::calculate_private_key();
    }

    rsa_traditional(const rsa_traditional& other) : base_class_type(other) { }

    rsa_traditional(rsa_traditional&& other) noexcept : base_class_type(other) { }

    virtual ~rsa_traditional() = default;

    auto operator=(const rsa_traditional& other) -> rsa_traditional& // NOLINT(cert-oop54-cpp)
    {
      static_cast<void>(base_class_type::operator=(other));

      return *this;
    }

    auto operator=(rsa_traditional&& other) noexcept -> rsa_traditional&
    {
      static_cast<void>(base_class_type::operator=(other));

      return *this;
    }
  };
} // namespace local_rsa

#if defined(WIDE_INTEGER_NAMESPACE)
auto WIDE_INTEGER_NAMESPACE::math::wide_integer::example012_rsa_crypto() -> bool
#else
auto ::math::wide_integer::example012_rsa_crypto() -> bool
#endif
{
  // Consider lines 25-30 in the file "KeyGen_186-3.rsp".

  // e    = 00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000100000001
  // seed = e5f707e49c4e7cc8fb202b5cd957963713f1c4726677c09b6a7f5dfe
  // p    = ff03b1a74827c746db83d2eaff00067622f545b62584321256e62b01509f10962f9c5c8fd0b7f5184a9ce8e81f439df47dda14563dd55a221799d2aa57ed2713271678a5a0b8b40a84ad13d5b6e6599e6467c670109cf1f45ccfed8f75ea3b814548ab294626fe4d14ff764dd8b091f11a0943a2dd2b983b0df02f4c4d00b413
  // q    = dacaabc1dc57faa9fd6a4274c4d588765a1d3311c22e57d8101431b07eb3ddcb05d77d9a742ac2322fe6a063bd1e05acb13b0fe91c70115c2b1eee1155e072527011a5f849de7072a1ce8e6b71db525fbcda7a89aaed46d27aca5eaeaf35a26270a4a833c5cda681ffd49baa0f610bad100cdf47cc86e5034e2a0b2179e04ec7
  // n    = d9f3094b36634c05a02ae1a5569035107a48029e39b3c6a1853817f063e18e761c0c538e55ff2c7e53d603bb35cabb3b8d07f82aa0afdeaf7441fcf6746c5bcaaa2cde398ad73edb9c340c3ffca559132581eaf8f65c13d02f3445a932a3e1fadb5912f7553edec5047e4d0ed06ee87effc549e194d38e06b73a971c961688ba2d4aa4f450d2523372f317d41d06f9f0360e962ce953a69f36c53c370799fcfba195e8f691ebe862f84ae4bbd7747bc14499bd0efffcdc7154325908355c2ffc5b3948b8102b33aa2420381470e4ee858380ff0eea58288516c263f6d51dbbd0e477d1393a0a3ee60e1fde4330856665bf522006608a6104c138c0f39e09c4c5
  // d    = 1bf009caddc664b4404d59711fde16d7c55822449de1c5a084d22ed5791fdaa37ea538867fc91a17e6856e277c2dedd70ca8bf6ec44b0e729917a88e5988cc561d948ddeea46e21fd8ff46cce7657c94bfb1bdf40b3b30d4595a8bc3a15f1d4ad4c665c09b3b265ba19cdb0b89cbaadd0097ff52e9f6e594f86829c5bb4e9ba0200f12fa6dc60fd28dec0d194f08deb50f5a7749540160d6e8338e75b11165b76f4650c2fcce08f979ad9941daedaa5e328473bf712f8f549c36967f5e15477dc643d1f48d563139134e5cdc4bb84f9782cd5125e864e067cb980290f215cb41090e297bac2714efba61115d85613851c2de50a82f4ab526b88c61b7c9a0b589

  using rsa_type          = local_rsa::rsa_fips<static_cast<std::size_t>(UINT32_C(2048))>;
  using rsa_integral_type = typename rsa_type::my_uintwide_t;

  const rsa_integral_type p("0xFF03B1A74827C746DB83D2EAFF00067622F545B62584321256E62B01509F10962F9C5C8FD0B7F5184A9CE8E81F439DF47DDA14563DD55A221799D2AA57ED2713271678A5A0B8B40A84AD13D5B6E6599E6467C670109CF1F45CCFED8F75EA3B814548AB294626FE4D14FF764DD8B091F11A0943A2DD2B983B0DF02F4C4D00B413");
  const rsa_integral_type q("0xDACAABC1DC57FAA9FD6A4274C4D588765A1D3311C22E57D8101431B07EB3DDCB05D77D9A742AC2322FE6A063BD1E05ACB13B0FE91C70115C2B1EEE1155E072527011A5F849DE7072A1CE8E6B71DB525FBCDA7A89AAED46D27ACA5EAEAF35A26270A4A833C5CDA681FFD49BAA0F610BAD100CDF47CC86E5034E2A0B2179E04EC7");
  const rsa_integral_type e("0x100000001");
  const rsa_integral_type n("0xD9F3094B36634C05A02AE1A5569035107A48029E39B3C6A1853817F063E18E761C0C538E55FF2C7E53D603BB35CABB3B8D07F82AA0AFDEAF7441FCF6746C5BCAAA2CDE398AD73EDB9C340C3FFCA559132581EAF8F65C13D02F3445A932A3E1FADB5912F7553EDEC5047E4D0ED06EE87EFFC549E194D38E06B73A971C961688BA2D4AA4F450D2523372F317D41D06F9F0360E962CE953A69F36C53C370799FCFBA195E8F691EBE862F84AE4BBD7747BC14499BD0EFFFCDC7154325908355C2FFC5B3948B8102B33AA2420381470E4EE858380FF0EEA58288516C263F6D51DBBD0E477D1393A0A3EE60E1FDE4330856665BF522006608A6104C138C0F39E09C4C5");
  const rsa_integral_type d("0x1BF009CADDC664B4404D59711FDE16D7C55822449DE1C5A084D22ED5791FDAA37EA538867FC91A17E6856E277C2DEDD70CA8BF6EC44B0E729917A88E5988CC561D948DDEEA46E21FD8FF46CCE7657C94BFB1BDF40B3B30D4595A8BC3A15F1D4AD4C665C09B3B265BA19CDB0B89CBAADD0097FF52E9F6E594F86829C5BB4E9BA0200F12FA6DC60FD28DEC0D194F08DEB50F5A7749540160D6E8338E75B11165B76F4650C2FCCE08F979AD9941DAEDAA5E328473BF712F8F549C36967F5E15477DC643D1F48D563139134E5CDC4BB84F9782CD5125E864E067CB980290F215CB41090E297BAC2714EFBA61115D85613851C2DE50A82F4AB526B88C61B7C9A0B589");

  bool result_is_ok = true;

  {
    using local_random_engine_type = std::mt19937;

    local_random_engine_type generator(static_cast<typename std::mt19937::result_type>(std::clock()));

    const bool p_is_prime = rsa_type::is_prime(p, generator);

    result_is_ok = (p_is_prime && result_is_ok);
  }

  result_is_ok = (rsa_type::is_prime(q) && result_is_ok);

  const rsa_type rsa(p, q, e);

  result_is_ok = ((   (rsa.get_p() == p)
                   && (rsa.get_q() == q)
                   && (rsa.get_d() == d)) && result_is_ok);

  result_is_ok = ((n == (p * q)) && result_is_ok);
  result_is_ok = ((n == rsa.get_n()) && result_is_ok);

  // Select "abc" as the sample string to encrypt.
  const std::string in_str("abc");

  const typename rsa_type::crypto_string out_str = rsa.encrypt(in_str);
  const std::string                     res_str = rsa.decrypt(out_str);

  const auto res_ch_a_manual = static_cast<char>(static_cast<typename rsa_integral_type::limb_type>(powm(out_str[0U], d, n)));
  const auto res_ch_b_manual = static_cast<char>(static_cast<typename rsa_integral_type::limb_type>(powm(out_str[1U], d, n)));
  const auto res_ch_c_manual = static_cast<char>(static_cast<typename rsa_integral_type::limb_type>(powm(out_str[2U], d, n)));

  result_is_ok = ((res_str         == "abc") && result_is_ok);
  result_is_ok = ((res_ch_a_manual == 'a')   && result_is_ok);
  result_is_ok = ((res_ch_b_manual == 'b')   && result_is_ok);
  result_is_ok = ((res_ch_c_manual == 'c')   && result_is_ok);

  return result_is_ok;
}

// Enable this if you would like to activate this main() as a standalone example.
#if defined(WIDE_INTEGER_STANDALONE_EXAMPLE012_RSA_CRYPTO)

#include <iomanip>
#include <iostream>

auto main() -> int
{
  #if defined(WIDE_INTEGER_NAMESPACE)
  const auto result_is_ok = WIDE_INTEGER_NAMESPACE::math::wide_integer::example012_rsa_crypto();
  #else
  const auto result_is_ok = ::math::wide_integer::example012_rsa_crypto();
  #endif

  std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;

  return (result_is_ok ? 0 : -1);
}

#endif
