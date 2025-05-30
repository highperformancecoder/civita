/*
  @copyright Russell Standish 2025
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

#include "tensorVal.h"
using namespace civita;

#include <UnitTest++/UnitTest++.h>

using namespace std;

SUITE(TensorVal)
{
  TEST_FIXTURE(TensorVal, assignDenseOrSpase)
  {
    Hypercube hc{3,3};
    map<size_t,double> sparseData{{1,1},{3,3},{8,8}};
    assign(hc,sparseData);
    CHECK_EQUAL(sparseData.size(),size());
    for (auto& i: sparseData) CHECK_EQUAL(i.second, atHCIndex(i.first));

    map<size_t,double> denseData{{0,0},{1,1},{2,2},{3,3},{4,4},{5,5}};
    assign(hc,denseData);
    CHECK_EQUAL(hc.numElements(), size());
    for (auto& i: denseData) CHECK_EQUAL(i.second, atHCIndex(i.first));
    for (size_t i=0; i<size(); ++i)
      if (!denseData.count(i))
        CHECK(isnan((*this)[i]));
  }
    
}
