/***************************************************************************
 *   Copyright 2018 by Davide Bettio <davide@uninstall.it>                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License as        *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#include "interop.h"
#include "tempstack.h"

char *interop_term_to_string(term t, int *ok)
{
    if (term_is_nonempty_list(t)) {
        return interop_list_to_string(t, ok);

    } else if (term_is_binary(t)) {
        char *str = interop_binary_to_string(t);
        *ok = str != NULL;
        return str;

    } else {
        //TODO: implement also for other types?
        *ok = 0;
        return NULL;
    }
}

char *interop_binary_to_string(term binary)
{
    int len = term_binary_size(binary);

    char *str = malloc(len + 1);
    if (IS_NULL_PTR(str)) {
        return NULL;
    }
    memcpy(str, term_binary_data(binary), len);

    str[len] = 0;

    return str;
}

char *interop_list_to_string(term list, int *ok)
{
    int proper;
    int len = term_list_length(list, &proper);
    if (UNLIKELY(!proper)) {
        *ok = 0;
        return NULL;
    }

    char *str = malloc(len + 1);
    if (IS_NULL_PTR(str)) {
        return NULL;
    }

    term t = list;
    for (int i = 0; i < len; i++) {
        term byte_value_term = term_get_list_head(t);
        if (UNLIKELY(!term_is_integer(byte_value_term))) {
            *ok = 0;
            free(str);
            return NULL;
        }

        if (UNLIKELY(!term_is_uint8(byte_value_term))) {
            *ok = 0;
            free(str);
            return NULL;
        }
        uint8_t byte_value = term_to_uint8(byte_value_term);

        str[i] = (char) byte_value;
        t = term_get_list_tail(t);
    }
    str[len] = 0;

    *ok = 1;
    return str;
}

term interop_proplist_get_value(term list, term key)
{
    return interop_proplist_get_value_default(list, key, term_nil());
}

term interop_proplist_get_value_default(term list, term key, term default_value)
{
    term t = list;

    while (!term_is_nil(t)) {
        term *t_ptr = term_get_list_ptr(t);

        term head = t_ptr[1];
        if (term_is_tuple(head) && term_get_tuple_element(head, 0) == key) {
            if (UNLIKELY(term_get_tuple_arity(head) != 2)) {
                break;
            }
            return term_get_tuple_element(head, 1);
        }

        t = *t_ptr;
    }

    return default_value;
}

int interop_iolist_size(term t, int *ok)
{
    if (term_is_binary(t)) {
        *ok = 1;
        return term_binary_size(t);
    }

    if (UNLIKELY(!term_is_list(t))) {
        *ok = 0;
        return 0;
    }

    unsigned long acc = 0;

    struct TempStack temp_stack;
    temp_stack_init(&temp_stack);

    temp_stack_push(&temp_stack, t);

    while (!temp_stack_is_empty(&temp_stack)) {
        if (term_is_integer(t)) {
            acc++;
            t = temp_stack_pop(&temp_stack);

        } else if (term_is_nil(t)) {
            t = temp_stack_pop(&temp_stack);

        } else if (term_is_nonempty_list(t)) {
            temp_stack_push(&temp_stack, term_get_list_tail(t));
            t = term_get_list_head(t);

        } else if (term_is_binary(t)) {
            acc += term_binary_size(t);
            t = temp_stack_pop(&temp_stack);

        } else {
            temp_stack_destory(&temp_stack);
            *ok = 0;
            return 0;
        }
    }

    temp_stack_destory(&temp_stack);

    *ok = 1;
    return acc;
}

int interop_write_iolist(term t, char *p)
{
    if (term_is_binary(t)) {
        int len = term_binary_size(t);
        memcpy(p, term_binary_data(t), len);
        return 1;
    }

    struct TempStack temp_stack;
    temp_stack_init(&temp_stack);

    temp_stack_push(&temp_stack, t);

    while (!temp_stack_is_empty(&temp_stack)) {
        if (term_is_integer(t)) {
            *p = term_to_int(t);
            p++;
            t = temp_stack_pop(&temp_stack);

        } else if (term_is_nil(t)) {
            t = temp_stack_pop(&temp_stack);

        } else if (term_is_nonempty_list(t)) {
            temp_stack_push(&temp_stack, term_get_list_tail(t));
            t = term_get_list_head(t);

        } else if (term_is_binary(t)) {
            int len = term_binary_size(t);
            memcpy(p, term_binary_data(t), len);
            p += len;
            t = temp_stack_pop(&temp_stack);

        } else {
            temp_stack_destory(&temp_stack);
            return 0;
        }
    }

    temp_stack_destory(&temp_stack);
    return 1;
}
