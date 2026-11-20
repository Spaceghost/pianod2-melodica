///
/// Parsnip serialization.
/// @file       parsnip_iterator.cpp - Iterators for serialization data type.
/// @author     Perette Barella
/// @date       2020-02-29
/// @copyright  Copyright 2020 Devious Fish. All rights reserved.
///

#include <config.h>

#include <cassert>

#include "parsnip.h"

namespace Parsnip {
    Data::iterator::iterator (const Data *l) : list (l->data.list) {
        if (l->datatype != Type::List) {
            throw IncorrectDataType (Type::List, l->datatype);
        }
    };

    Data::iterator::iterator (const Data *l, size_type pos) : list (l->data.list), position (pos) {
        if (l->datatype != Type::List) {
            throw IncorrectDataType (Type::List, l->datatype);
        }
        assert (pos == list->size());
    };
    
    const Data &Data::iterator::operator*() const noexcept {
        assert (list);
        assert (position >= 0 && position < list->size());
        return (*list)[position];
    }
    
    Data::iterator &Data::iterator::operator++() noexcept {
        assert (position >= 0 && position < list->size());
        position++;
        return *this;
    }
    Data::iterator Data::iterator::operator++ (int) noexcept {
        assert (position >= 0 && position < list->size());
        iterator prior = *this;
        position++;
        return prior;
    }
    
    bool Data::iterator::operator!= (const iterator &compare) const noexcept {
        assert (list == compare.list);
        return position != compare.position;
    }
}  // namespace Parsnip
