namespace ecs {
namespace type_utils {

struct GetTypeIndexVisitor {
  template <typename T> std::type_index operator()(T &&) { return typeid(T); }
};

template <class V> std::type_index get_variant_type(V const &v) { return std::visit(GetTypeIndexVisitor{}, v); }

template <typename T> struct type_identity { using type = T; };

} // namespace type_utils
} // namespace ecs