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

#include "xvector.h"
#include <regex>

using namespace std;

#include <boost/date_time.hpp>
using namespace boost::posix_time;
using namespace boost::gregorian;

#include <time.h>

namespace civita
{

  namespace
  {
    void extract(const string& fmt, const string& data, int pos1, const char* re1, int& var1,
                 int pos2, const char* re2, int& var2)
    {
      string rePat="\\s*"+fmt.substr(0,pos1)+re1+
        fmt.substr(pos1+2,pos2-pos1-2)+re2+
        fmt.substr(pos2+2)+"\\s*";
      regex pattern(rePat);
      smatch match;
      if (regex_match(data,match,pattern))
        {
          var1=stoi(match[1]);
          var2=stoi(match[2]);
        }
      else
        throw runtime_error("data "+data+" fails to match pattern "+rePat);
    }

    struct InvalidDate: public std::runtime_error
    {
      InvalidDate(const std::string& dateString, const std::string& format):
        runtime_error("invalid date/time: "+dateString+" for format "+format) {}
    };
  }

  
  void XVector::push_back(const std::string& s)
  {
    V::push_back(anyVal(dimension, s));
  }

  void AnyVal::setDimension(const Dimension& dim)
  {
    this->dim=dim;
    switch (dim.type)
      {
      case Dimension::string:
      case Dimension::value:
        return;
      case Dimension::time:
        if ((pq=dim.units.find("%Q"))!=string::npos)
          {
            timeType=quarter;
            return;
          }
        // handle date formats with any combination of %Y, %m, %d, %H, %M, %S
        // handle dates with 1 or 2 digits see Ravel ticket #35.
        // Delegate to std::time_facet if time fields abut or more complicated formatting is requested
        static regex nonStandardDateTimes{"%[^mdyYHMS]|%[mdyYHMS]%[mdyYHMS]"};
        smatch match;
        if (!regex_search(dim.units, match,nonStandardDateTimes)) 
          {
            timeType=regular;
            static regex thisTimeFormat{"%([mdyYHMS])"};
            for (string formatStr=dim.units.empty()? "%Y %m %d %H %M %S": dim.units;
                 regex_search(formatStr, match, thisTimeFormat);
                 formatStr=match.suffix())
              {format.push_back(match.str(1)[0]);}
            return;
          }
        timeType=time_input_facet;
        return;
      }
  }

  any AnyVal::constructAnyFromQuarter(const std::string& s) const
  {
    // year quarter format expected. Takes the first %Y (or
    // %y) and first %Q for year and quarter
    // respectively. Everything else is passed to regex, which
    // can be used to match complicated patterns.
    string pattern;
    static greg_month quarterMonth[]={Jan,Apr,Jul,Oct};
    int year, quarter;
    auto pY=dim.units.find("%Y");
    if (pY!=string::npos)
      if (pq<pY)
        extract(dim.units,s,pq,"(\\d)",quarter,pY,"(\\d{4})",year);
      else
        extract(dim.units,s,pY,"(\\d{4})",year,pq,"(\\d)",quarter);
    else
      throw runtime_error("year not specified in format string");
    if (quarter<1 || quarter>4)
      throw runtime_error("invalid quarter "+to_string(quarter));
    return ptime(date(year, quarterMonth[quarter-1], 1));
  }
    
  // handle date formats with any combination of %Y, %m, %d, %H, %M, %S
  any AnyVal::constructAnyFromRegular(const std::string& s) const
  {
    smatch val;
    static regex valParser{"(\\d+)"};
    int day=1, month=1, year=0, hours=0, minutes=0, seconds=0;
    size_t i=0;
    for (auto ss=s.c_str(); i<format.size(); ++i)
      {
        for (; *ss && !isdigit(*ss); ++ss); // skip to next integer field
        if (!*ss) break;
        int v=strtol(ss, const_cast<char**>(&ss),10);
        switch (format[i])
          {
          case 'd': day=v; break;
          case 'm': month=v; break;
          case 'y':
            if (v>99) throw runtime_error(to_string(v)+" is out of range for %y");
            year=v>68? v+1900: v+2000;
            break;
          case 'Y': year=v; break;
          case 'H': hours=v; break;
          case 'M': minutes=v; break;
          case 'S': seconds=v; break;
          }
      }
    if (!dim.units.empty() && i<format.size()) throw InvalidDate(s, dim.units);
    return ptime(date(year,month,day),time_duration(hours,minutes,seconds));
  }
  
  any AnyVal::operator()(const std::string& s) const
  {
    switch (dim.type)
      {
      case Dimension::string:
        if (s.empty()) return " "; // empty strings have a special meaning, so on construction, replace with a blank string
        return s;
      case Dimension::value:
        if (s.empty()) return nan("");
        return stod(s);
      case Dimension::time:
        if (s.empty()) return not_a_date_time;
        switch (timeType)
          {
          case quarter: return constructAnyFromQuarter(s);
          case regular: return constructAnyFromRegular(s);
          case time_input_facet:
            {
              // default case - delegate to std::time_input_facet
              istringstream is(s);
              is.imbue(locale(is.getloc(), new boost::posix_time::time_input_facet(dim.units)));
              ptime pt(not_a_date_time);
              is>>pt;
              if (pt.is_special())
                throw InvalidDate(s, dim.units);
              return pt;
            }
            // note: boost time_input_facet too restrictive, so this was a strptime attempt. See Ravel ticket #35
            // strptime is not available on Windows alas
          }
      }

    assert(false);
    return any(); // shut up compiler warning
  }

  double diff(const any& x, const any& y)
  {
    if (x.type!=y.type)
#ifdef CLASSDESC_H
      throw runtime_error("incompatible types "+classdesc::to_string(x.type)+" and "+classdesc::to_string(y.type)+" in diff");
#else
    throw runtime_error("incompatible types in diff");
#endif
    switch (x.type)
      {
      case Dimension::string:
        {
          // Hamming distance
          double r=abs(double(x.string.length())-double(y.string.length()));
          for (size_t i=0; i<x.string.length() && i<y.string.length(); ++i)
            r += x.string[i]!=y.string[i];
          return x.string<y.string? -r: r;
        }
      case Dimension::value:
        return x.value-y.value;
      case Dimension::time:
        {
          auto d=x.time-y.time;
          auto cutoff=hours(1000000);
          if (d<cutoff && d>-cutoff) // arbitrary cutoff, but well below overflow - million hours = a bit over a century
            return 1e-9*d.total_nanoseconds();
          return 1e-6*d.total_microseconds();
        }
      }
    assert(false); 
    return 0; // should not be here
  }

  // format a string with two integer arguments
  string formatString(const std::string& format, int i, int j)
  {
    char r[512];
    snprintf(r,sizeof(r),format.c_str(),i,j);
    return r;
  }
  
  string str(const any& v, const string& format)
  {
    switch (v.type)
      {
      case Dimension::string: return v.string;
      case Dimension::value: return to_string(v.value);
      case Dimension::time:
        {
          string::size_type pq;
          if (format.empty())
            return to_iso_extended_string(v.time);
          if ((pq=format.find("%Q"))!=string::npos)
            {
              auto pY=format.find("%Y");
              if (pY==string::npos)
                throw runtime_error("year not specified in format string");
            
              // replace %Q and %Y with %d
              string sformat=format;
              for (size_t i=1; i<sformat.size(); ++i)
                if (sformat[i-1]=='%' && (sformat[i]=='Q' || sformat[i]=='Y'))
                  sformat[i]='d';
              char result[100];
              auto tm=to_tm(v.time.date());
              if (pq<pY)
                return formatString(sformat,tm.tm_mon/3+1, tm.tm_year+1900);
              return formatString(sformat, tm.tm_year+1900, tm.tm_mon/3+1);
            }
          else
            {
              unique_ptr<time_facet> facet(new time_facet(format.c_str()));
              ostringstream os;
              os.imbue(locale(os.getloc(), facet.release()));
              os<<v.time;
              return os.str();
            }
        }
      }
    assert(false); // shouldn't be here
    return {};
  }
  
  string XVector::timeFormat() const
  {
    if (dimension.type!=Dimension::time || empty()) return "";
    static const auto day=hours(24);
    static const auto month=day*30;
    static const auto year=day*365;
    auto f=front().time, b=back().time;
    if (f>b) std::swap(f,b);
    auto dt=b-f;
    if (dt > year*5)
      return "%Y";
    if (dt > year)
      return "%b %Y";
    if (dt > month*6)
      return "%b";
    if (dt > month)
      return "%d %b";
    if (dt > day)
      return "%d %H:%M";
    if (dt > hours(1))
      return "%H:%M";
    if (dt > minutes(1))
      return "%M:%S";
    return "%s";
  }
  
  void XVector::imposeDimension()
  {
    // check if anything to be done
    switch (dimension.type)
      {
      case Dimension::string:
        if (checkType<string>()) return;
        break;
      case Dimension::value:
        if (checkType<double>()) return;
        break;
      case Dimension::time:
        if (checkType<ptime>()) return;
        break;
      }

    for (auto& i: *this)
      i=anyVal(dimension, str(i));
    assert(checkThisType());
  }

}

#ifdef CLASSDESC
namespace classdesc
{
  void json_pack(json_unpack_t& j, const std::string&, civita::XVector& x)
  {
    json_pack(j,"",static_cast<civita::NamedDimension&>(x));
    vector<string> sliceLabels;
    for (auto& i: x)
      sliceLabels.push_back(str(i, x.dimension.units));
    json_pack(j,".slices",sliceLabels);
  }
  void json_unpack(json_unpack_t& j, const std::string&, civita::XVector& x)
  {
    x.clear();
    json_unpack(j,"",static_cast<civita::NamedDimension&>(x));
    vector<string> slices;
    json_unpack(j,".slices",slices);
    for (auto& i: slices) x.push_back(i);
  }
}
#include "dimension.jcd"
#include "xvector.jcd"
#include <classdesc_epilogue.h>
#endif
