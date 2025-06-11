#pragma once


namespace compressed_py
{

	// variants for our type-erased(ish) classes we expose on the python side.
	template <template<typename> class Class>
	using variant_t = std::variant<
		Class<float>		// python equivalent: np.float32
		Class<double>		// python equivalent: np.float64
		Class<uint8_t>		// python equivalent: np.uint8
		Class<int8_t>		// python equivalent: np.int8
		Class<uint16_t>		// python equivalent: np.uint16
		Class<int16_t>		// python equivalent: np.int16
		Class<uint32_t>		// python equivalent: np.uint32
		Class<int32_t>		// python equivalent: np.int32
	>;


	template <template<typename> class Class>
	class base_variant_class
	{
	protected:
		variant_t<Class> class_variant;

	public:

		base_variant_class() = default;

		base_variant_class(variant_t<Class> variant)
			: class_variant(std::move(variant)) {
		}

		py::dtype dtype() const {
			return std::visit([](const auto& var) {
				using T = typename std::decay_t<decltype(var)>::value_type;
				return py::dtype::of<T>();
				}, class_variant);
		}

	};

}