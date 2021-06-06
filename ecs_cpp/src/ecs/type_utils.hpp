namespace ecs {
namespace type_utils {


std::atomic_int TypeIdCounter;

template<typename T>
std::size_t get_type_id() {
    static std::size_t id = TypeIdCounter++;
    return id;
}

struct GetTypeIndexVisitor {
  template <typename T> std::size_t operator()(T &&) { return get_type_id<T>(); }
};

template <class V> std::size_t get_variant_type(V const &v) { return std::visit(GetTypeIndexVisitor{}, v); }

template <typename T> struct type_identity { using type = T; };

} // namespace type_utils
} // namespace ecs