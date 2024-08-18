/*
  @copyright Russell Standish 2019
  @author Russell Standish
  This file is part of Civita.

  Civita is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Civita is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Civita.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CIVITA_DIMENSION_H
#define CIVITA_DIMENSION_H
#include <boost/date_time.hpp>
#include <string>
#include <map>
#include <stdexcept>

namespace civita
{
  struct Dimension
  {
    enum Type {string, time, value};
    Type type=string;
    std::string units; // for values, or parser string for time conversion
    Dimension() {}
    Dimension(Type t,const std::string& s): type(t), units(s) {}
  };

  template <class T> Dimension::Type dimensionTypeOf();
  template <> inline Dimension::Type dimensionTypeOf<std::string>() {return Dimension::string;}
  template <> inline Dimension::Type dimensionTypeOf<boost::posix_time::ptime>() {return Dimension::time;} 
  template <> inline Dimension::Type dimensionTypeOf<double>() {return Dimension::value;}

  /// a variant type representing a value of a dimension
  // TODO - when we move to c++17, consider using std::variant
  struct any
  {
    Dimension::Type type=Dimension::string;
    boost::posix_time::ptime time;
    double value;
    std::string string;
    size_t hash() const;
    any()=default;
    any(Dimension::Type type): type(type) {}
    any(const boost::posix_time::ptime& x): type(Dimension::time), time(x) {}
    any(const std::string& x): type(Dimension::string), string(x) {}
    any(const char* x): type(Dimension::string), string(x) {}
    // compilers get confused between char* and ints, which is why we need this templated overload
    template <class T>
    any(T x, typename std::enable_if<std::is_integral<T>::value, int>::type dummy=0):
      type(Dimension::value), value(x) {}
    any(double x): type(Dimension::value), value(x) {}
    template <class T> any& operator=(const T&x) {return *this=any(x);}
    /// true if this is a default constructed object
    bool empty() const {return type==Dimension::string && string.empty();}
  };

  inline size_t any::hash() const {
    switch (type) {
    case Dimension::string: return std::hash<std::string>()(string);
    case Dimension::time: return std::hash<size_t>()((time-boost::posix_time::ptime()).ticks());
    case Dimension::value: return std::hash<double>()(value);
    }
    assert(false);
    return 0;
  }

  inline bool operator<(const any& x, const any& y) {
    if (x.type==y.type)
      switch (x.type)   {
      case Dimension::string: return x.string<y.string;
      case Dimension::time: return x.time<y.time;
      case Dimension::value: return x.value<y.value;
      }
    return x.type<y.type;
  }
  
  inline bool operator<=(const any& x, const any& y) {
    if (x.type==y.type)
      switch (x.type)   {
      case Dimension::string: return x.string<=y.string;
      case Dimension::time: return x.time<=y.time;
      case Dimension::value: return x.value<=y.value;
      }
    return x.type<y.type;
  }
  
  inline bool operator>(const any& x, const any& y) {return !(x<=y);}
  inline bool operator>=(const any& x, const any& y) {return !(x<y);}
  
  inline bool operator==(const any& x, const any& y) {
    if (x.type!=y.type) return false;
    switch (x.type)   {
    case Dimension::string: return x.string==y.string;
      // peculiar syntax to work around compiler bug in implementing C++20 ambiguity rules
    case Dimension::time: return x.time.operator==(y.time); 
    case Dimension::value: return x.value==y.value;
    }
    assert(false);
    return false; // should never be here
  }

  inline bool operator!=(const any& x, const any& y) {
    return !(x==y);
  }

  /// interpolate betwwen x and y with fraction a (between 0 & 1)
  /// if x&y are different types or are strings, return x
  inline any interpolate(const any& x, const any& y, double a)
  {
    if (x.type!=y.type) return x;
    switch (x.type)
      {
      case Dimension::string: return a<=0.5? x: y;
      case Dimension::value: return y.value*a+x.value*(1-a);
      case Dimension::time: return x.time + (y.time-x.time)*a;
      }
    assert(false);
    return {}; // unreachable code to satisfy CodeQL
  }
  
#ifdef CLASSDESC_STRINGKEYMAP_H
  using classdesc::StringKeyMap;
#else
  template <class T> using StringKeyMap=std::map<std::string, T>;
#endif

  typedef StringKeyMap<Dimension> Dimensions;

  typedef std::map<std::string, double> ConversionsMap;
  struct Conversions: public ConversionsMap
  {
    double convert(double val, const std::string& from, const std::string& to)
    {
      if (from==to) return val;
      auto i=find(from+":"+to);
      if (i!=end())
        return i->second*val;
      i=find(to+":"+from);
      if (i!=end())
        return val/i->second;
      throw std::runtime_error("inconvertible types "+from+" and "+to);
    }
    Conversions& operator=(const ConversionsMap& x)
    {ConversionsMap::operator=(x); return *this;}
  };
  
  /// \a format - can be any format string suitable for a
  /// boost::date_time time_facet. eg "%Y-%m-%d %H:%M:%S"
  std::string str(const any&, const std::string& format="");

  inline std::ostream& operator<<(std::ostream& o, const any& x)
  {return o<<str(x);}
}

#ifdef CLASSDESC
#pragma omit pack civita::any
#pragma omit unpack civita::any
#pragma omit json_pack civita::any
#pragma omit json_unpack civita::any
#pragma omit RESTProcess civita::any
#include <json_pack_base.h>
#include <pack_base.h>
#include <random_init_base.h>
#include <RESTProcess_base.h>
namespace classdesc_access
{
#ifdef CLASSDESC_JSON_PACK_BASE_H
  template <>
  struct access_json_pack<civita::any>
  {
    inline void operator()(classdesc::json_pack_t& j, const std::string&, const civita::any& x)
    {j<<civita::str(x);}
  };
  template <>
  struct access_json_unpack<civita::any>: public classdesc::NullDescriptor<classdesc::json_pack_t> {};
#endif
  
#ifdef CLASSDESC_RESTPROCESS_BASE_H
  template <>
  struct access_RESTProcess<civita::any>: public classdesc::NullDescriptor<classdesc::RESTProcess_t> {};
#endif
  
  template <>
  struct access_pack<civita::any>
  {
    inline void operator()(classdesc::pack_t& b, const std::string&, const civita::any& x)
    {
      b<<static_cast<int>(x.type);
      switch (x.type)
        {
        case civita::Dimension::string:
          b<<x.string;
          break;
        case civita::Dimension::value:
          b<<x.value;
          break;
        case civita::Dimension::time:
          b<<x.time;
          break;
        }
    }
  };
  
  template <>
  struct access_unpack<civita::any> {
    inline void operator()(classdesc::pack_t& b, const std::string&, civita::any& x)
    {
      int type;
      b>>type;
      x.type=static_cast<civita::Dimension::Type>(type);
      switch(x.type)
        {
        case civita::Dimension::string:
          b>>x.string;
          break;
        case civita::Dimension::value:
          b>>x.value;
          break;
        case civita::Dimension::time:
          b>>x.time;
          break;
        }
    }
  };

  template <>
  struct access_pack<boost::posix_time::ptime> {
    template <class T>
    void operator()(classdesc::pack_t& b,const std::string&,T& x)
    {
      auto tm=to_tm(x);
      b.packraw(reinterpret_cast<char*>(&tm),sizeof(tm));
    }
  };
  template <>
  struct access_unpack<boost::posix_time::ptime> {
    template <class T>
    void operator()(classdesc::pack_t& b,const std::string&,T& x)
    {
      struct tm tm;
      b.unpackraw(reinterpret_cast<char*>(&tm),sizeof(tm));
      x=boost::posix_time::ptime_from_tm(tm);
    }
  };

  template <>
  struct access_random_init<boost::posix_time::ptime> {
    void operator()(classdesc::random_init_t& r, const std::string&, boost::posix_time::ptime& x)
    {x=boost::posix_time::from_time_t(time_t(r.rand()*double(std::numeric_limits<time_t>::max())));}
  };
}
#endif

#endif
