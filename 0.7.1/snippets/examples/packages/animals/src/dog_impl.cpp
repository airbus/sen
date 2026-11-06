// === dog_impl.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// generated code
#include "stl/dog.stl.h"

// package
#include "animal.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/var.h"

// generated code
#include "stl/animal.stl.h"

// std
#include <string>

namespace animals
{

class DogImpl: public DogBase<AnimalImpl>
{
public:
  SEN_NOCOPY_NOMOVE(DogImpl)

public:
  DogImpl(const std::string& name, const sen::VarMap& params): DogBase<AnimalImpl>(name, params) { init(); }

  ~DogImpl() override = default;

protected:
  void goodDogImpl() override { madeSound("wuff!"); }

  void badDogImpl() override { madeSound("grrr!"); }

private:
  void init()
  {
    Taxonomy taxonomy;
    taxonomy.domain = "Eukaryota";
    taxonomy.kingdom = "Animalia";
    taxonomy.phylum = "Chordata";
    taxonomy.type = "Mammalia";
    taxonomy.order = "Carnivora";
    taxonomy.family = "Canidae";
    taxonomy.genus = "Canis";
    taxonomy.species = "C. familiaris";

    setNextTaxonomy(taxonomy);
  }
};

SEN_EXPORT_CLASS(DogImpl)

}  // namespace animals
