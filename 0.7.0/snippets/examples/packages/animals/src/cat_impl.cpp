// === cat_impl.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// generated code
#include "stl/cat.stl.h"

// package
#include "animal.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/var.h"

// generated code
#include "stl/animal.stl.h"

// std
#include <cstddef>
#include <string>

namespace animals
{

class CatImpl: public CatBase<AnimalImpl>
{
public:
  SEN_NOCOPY_NOMOVE(CatImpl)

public:
  CatImpl(const std::string name, const sen::VarMap& params): CatBase<AnimalImpl>(name, params) { init(); }

  ~CatImpl() override = default;

protected:
  void jumpToLocationImpl(MetersU32 x, MetersU32 y) override
  {
    jumpCount_++;
    setNextPosition({x, y});

    if (jumpCount_ % 3 == 0)
    {
      madeSound("meow!");
    }
  }

private:
  void init()
  {
    Taxonomy taxonomy;
    taxonomy.domain = "Eukaryota";
    taxonomy.kingdom = "Animalia";
    taxonomy.phylum = "Chordata";
    taxonomy.type = "Mammalia";
    taxonomy.order = "Carnivora";
    taxonomy.family = "Felidae";
    taxonomy.genus = "Felis";
    taxonomy.species = "F. catus";

    setNextTaxonomy(taxonomy);
  }

private:
  std::size_t jumpCount_ = 0U;
};

SEN_EXPORT_CLASS(CatImpl)

}  // namespace animals
