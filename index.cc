/*
  @copyright Russell Standish 2020
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


#include "index.h"
#include <algorithm>
#include <atomic>
#include <assert.h>
// Windows byte definition clashes with std::byte, even though we don't use it.
using std::atomic;
using std::bad_alloc;
using std::size_t;

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

#if defined(__linux__)
#include <sys/sysinfo.h>
#endif

#ifdef __APPLE__
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

using namespace std;

namespace civita
{
  size_t Index::linealOffset(size_t h) const
  {
    auto it=std::find(index.begin(), index.end(), h);
    if (it!=index.end()) return size_t(it-index.begin());
    return index.size();
  }
  
  size_t physicalMem() 
  {
#if defined(__linux__)
    struct sysinfo s;
    sysinfo(&s);
    return s.totalram;
#elif defined(WIN32)
    MEMORYSTATUSEX s{sizeof(MEMORYSTATUSEX)};
    GlobalMemoryStatusEx(&s);
    return s.ullTotalPhys;
#elif defined(__APPLE__)
    int mib[]={CTL_HW,HW_MEMSIZE};
    uint64_t physical_memory;
    size_t length = sizeof(uint64_t);
    if (sysctl(mib, sizeof(mib)/sizeof(int), &physical_memory, &length, NULL, 0))
      perror("physicalMem:");
    return physical_memory;
#else
    // all else fails, return max value
    return ~0UL;
#endif  
  }
  
  void trackAllocation(ptrdiff_t n)
  {
    static atomic<size_t> allocated{0}, max_allocated{0};
    // discount factor determined empirically to prevent application being pushed into swap
    static size_t memAvailable=0.6*physicalMem();
    // debug code left for posterity
    // auto m=max_allocated.load();
    // while (allocated>m)
    //   if (max_allocated.compare_exchange_weak(m,allocated))
    //     cout<<max_allocated<<endl;
    if (n>0 && allocated+n>memAvailable) // limit allocations to physical memory
      throw bad_alloc();
    if (-n<ptrdiff_t(allocated)) // reset allocated to 0 if n would reduce it to a -ve number
      allocated+=n;
    else
      allocated=0;
  }
}
