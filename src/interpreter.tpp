
/// Macro for a case in a switch statement to perform the given operation with the given type, if value is of that type
#define OP_WITH(type_, op_) case BasicRuntimeType<type_>::runtime_type: { \
    typedef BasicRuntimeType<type_>::type& rhs_type; \
    typedef decltype(value op_ dynamic_cast<rhs_type>(right).m_value) result_type; \
    typedef typename BasicRuntimeType<result_type>::type result_runtime_type; \
    return std::make_unique<result_runtime_type>(value op_ dynamic_cast<rhs_type>(right).m_value); \
}


/// Basic binary operations
#define BASIC_OP(op_) template<typename T, RuntimeType TYPE> \
spRuntimeValue BasicRuntimeValue<T, TYPE>::operator op_(RuntimeValue &right) { \
    T &value = m_value; \
    switch (right.m_type) { \
        OP_WITH(int32_t, op_) \
        OP_WITH(int64_t, op_) \
        OP_WITH(float, op_) \
        OP_WITH(double, op_) \
        OP_WITH(bool, op_) \
        default: \
            throw "Undefined operator " #op_; \
    } \
}
BASIC_OP(+)
BASIC_OP(-)
BASIC_OP(*)
BASIC_OP(/)
BASIC_OP(&&)
BASIC_OP(||)
BASIC_OP(==)
BASIC_OP(!=)
BASIC_OP(<)
BASIC_OP(>)
BASIC_OP(<=)
BASIC_OP(>=)
#undef BASIC_OP


/// Integer operations
#define INT_OP(op_, op_name_) template<typename T, bool = std::is_integral<T>::value> \
struct operator_##op_name_ {}; \
template<typename T> \
struct operator_##op_name_<T, false> { \
    inline spRuntimeValue operator()(T value, RuntimeValue &right) { \
        throw "Undefined operator " #op_; \
    } \
}; \
template<typename T> \
struct operator_##op_name_<T, true> { \
    inline spRuntimeValue operator()(T value, RuntimeValue &right) { \
        switch (right.m_type) { \
            OP_WITH(int32_t, op_) \
            OP_WITH(int64_t, op_) \
            OP_WITH(bool, op_) \
            default: \
                throw "Undefined operator " #op_; \
        } \
    } \
}; \
template<typename T, RuntimeType TYPE> \
spRuntimeValue BasicRuntimeValue<T, TYPE>::operator op_(RuntimeValue &right) { \
    return operator_##op_name_<T>()(m_value, right); \
}
INT_OP(%, mod)
INT_OP(&, and)
INT_OP(|, or)
INT_OP(^, xor)
INT_OP(<<, left_shift)
INT_OP(>>, right_shift)
#undef INT_OP


/// Unary operators
#define UNARY_OP(op_) template<typename T, RuntimeType TYPE> \
spRuntimeValue BasicRuntimeValue<T, TYPE>::operator op_() { \
    return std::make_unique<typename BasicRuntimeType<decltype(op_ m_value)>::type>(op_ m_value); \
}
UNARY_OP(!)
UNARY_OP(+)
UNARY_OP(-)
#undef UNARY_OP


#undef OP_WITH



/// Special case: integer bitwise not operation
template<typename T, bool = std::is_integral<T>::value>
struct operator_bitnot {};
template<typename T>
struct operator_bitnot<T, false> {
    inline spRuntimeValue operator()(T value) {
        throw "Undefined operator ~";
    }
};
template<typename T>
struct operator_bitnot<T, true> {
    inline spRuntimeValue operator()(T value) {
        typedef typename BasicRuntimeType<decltype(~value)>::type result_type;
        return std::make_unique<result_type>(~value);
    }
};
template<typename T, RuntimeType TYPE>
spRuntimeValue BasicRuntimeValue<T, TYPE>::operator~() {
    return operator_bitnot<T>()(m_value);
}
