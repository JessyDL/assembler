#pragma once
#include <memory>
#include <optional>
#include "vector.h"
#include "array.h"
#include "ustring.h"
#include "string_utils.h"
namespace psl::cli
{
	template <typename T>
	class value;

	class pack;

	namespace details
	{
		static psl::static_array<psl::string8::char_t, 4> illegal_characters{' ', '"', '\'', '-'};
		using type_key_t = const std::uintptr_t* (*)();
		template <typename T>
		constexpr const std::uintptr_t type_key_var{0u};

		template <typename T>
		constexpr const std::uintptr_t* type_key() noexcept
		{
			return &type_key_var<T>;
		}

		template <typename T>
		constexpr type_key_t key_for()
		{
			return type_key<typename std::decay<T>::type>;
		};

		class value_base
		{
		  public:
			value_base(psl::string8_t name, const psl::array<psl::string8_t>& commands, bool optional, type_key_t id)
				: m_Name(name), m_Optional(optional), m_ID(id)
			{
				for(const auto& command : commands)
				{
					validate(command);
					if(command.size() == 1)
					{
						m_ShortCommands.emplace_back(command[0]);
						continue;
					}
					m_Commands.emplace_back(command);
				}
			}
			virtual ~value_base() = default;
			bool optional() const noexcept { return m_Optional; };
			const psl::string8_t& name() const noexcept { return m_Name; }
			psl::string8_t name() noexcept { return m_Name; }
			bool contains_command(psl::string8::view command, bool include_short_commands = true) const noexcept
			{
				return std::find(std::begin(m_Commands), std::end(m_Commands), command) != std::end(m_Commands) ||
					   (include_short_commands && command.size() == 1 && contains_short_command(command[0]));
			}
			bool contains_short_command(psl::string8::char_t command) const noexcept
			{
				return std::find(std::begin(m_ShortCommands), std::end(m_ShortCommands), command) !=
					   std::end(m_ShortCommands);
			}

			template <typename T>
			value<T>& as()
			{
				if(m_ID != key_for<T>()) throw std::runtime_error("incorrect cast");

				return *dynamic_cast<value<T>*>(this);
			}

			bool conflicts(const value_base& other) const noexcept
			{
				auto cmds		= commands();
				auto other_cmds = other.commands();

				std::sort(std::begin(cmds), std::end(cmds));
				std::sort(std::begin(other_cmds), std::end(other_cmds));
				psl::array<psl::string8_t> res;
				std::set_intersection(std::begin(cmds), std::end(cmds), std::begin(other_cmds), std::end(other_cmds),
									  std::back_inserter(res));
				return res.size() > 0;
			}

			virtual void from_string(psl::string_view t){};

		  protected:
			psl::array<psl::string8_t> commands() const noexcept
			{
				auto cpy = m_Commands;
				std::for_each(
					std::begin(m_ShortCommands), std::end(m_ShortCommands),
					[&cpy](psl::string8::char_t character) { cpy.emplace_back(psl::string8_t(&character, 1)); });
				return cpy;
			}

		  private:
			void validate(const psl::string8_t& command)
			{
				std::for_each(std::begin(illegal_characters), std::end(illegal_characters),
							  [&command](psl::string8::char_t character) {
								  if(std::find(std::begin(command), std::end(command), character) != std::end(command))
									  throw std::runtime_error("invalid character in command, a '" +
															   psl::string8_t(&character, 1) + "' was detected");
							  });
			}

			psl::string8_t m_Name;
			psl::array<psl::string8_t> m_Commands;
			psl::array<psl::string8::char_t> m_ShortCommands;
			bool m_Optional;
			type_key_t m_ID;
		};
	} // namespace details

	template <typename T>
	class value final : public details::value_base
	{
	  public:
		value(psl::string8_t name, const psl::array<psl::string8_t>& commands,
			  std::optional<std::shared_ptr<T>> default_value = std::nullopt, bool optional = true)
			: details::value_base(name, commands, optional, details::key_for<T>()), m_Default(default_value){};
		value(psl::string8_t name, const psl::array<psl::string8_t>& commands, T default_value, bool optional = true)
			: details::value_base(name, commands, optional, details::key_for<T>()),
			  m_Default(std::make_shared<T>(default_value)){};

		bool has_value() const noexcept { return m_Value || m_Default; }

		const T& get() const
		{
			if(m_Value) return *(m_Value.value());
			if(m_Default) return *(m_Default.value());

			throw std::runtime_error("no value found");
		}

		std::shared_ptr<T> get_shared()
		{
			if(!m_Value && m_Default)
			{
				m_Value = std::make_shared<T>(*m_Default.value());
			}

			if(!m_Value) throw std::runtime_error("no value found");

			return m_Value.value();
		}

		void set(const T& val)
		{
			if(!m_Value)
			{
				m_Value = std::make_shared<T>(val);
			}
			else
			{
				*(m_Value.value()) = val;
			}
		}

		void clear() { m_Value = std::nullopt; }

		void set_default(const T& val) { m_Default = std::make_shared<T>(val); }
		void set_default(std::shared_ptr<T>& val) { m_Default = val; }

		value<T> copy(bool linked_default = false) const noexcept
		{
			value<T> cpy{name(), commands(),
						 ((linked_default) ? m_Default
										   : ((m_Default) ? m_Default.value() : std::optional<std::shared_ptr<T>>{})),
						 optional()};
			if(m_Value) cpy.set(*m_Value.value());
			return cpy;
		}

		void from_string(psl::string_view string) override
		{
			if constexpr(!std::is_same<pack, T>::value)
			{
				if(string.size() == 0)
				{
					if constexpr(std::is_same<bool, T>::value)
					{
						set((m_Default) ? !(*m_Default.value()) : true);
					}
				}

				set(utility::converter<T>::from_string(psl::to_string8_t(string)));
			}
		}

	  private:
		std::optional<std::shared_ptr<T>> m_Value;
		std::optional<std::shared_ptr<T>> m_Default;
	};

	class pack final
	{
	  public:
		template <typename Fn, typename... Ts,
				  typename = std::enable_if_t<!(std::is_same<pack, std::decay_t<Fn>>::value && sizeof...(Ts) == 0)>>
		pack(Fn&& fn, Ts&&... values)
		{
			if constexpr(std::is_invocable<Fn, pack&>::value)
			{
				m_Callback = fn;
			}
			else
			{
				add(std::forward<Fn>(fn));
			}
			(add(std::forward<Ts>(values)), ...);
		}
		~pack()			  = default;
		pack(const pack&) = default;
		pack(pack&&)	  = default;
		pack& operator=(const pack&) = default;
		pack& operator=(pack&&) = default;

		auto begin() noexcept { return std::begin(m_Values); }
		auto end() noexcept { return std::end(m_Values); }
		auto cbegin() const noexcept { return std::cbegin(m_Values); }
		auto cend() const noexcept { return std::cend(m_Values); }

		std::shared_ptr<details::value_base> operator[](const psl::string8_t& name) const
		{
			for(const auto& v : m_Values)
			{
				if(v->name() == name) return v;
			}
			throw std::runtime_error("could not find value");
		}

		void parse(psl::array<psl::string_view> commands)
		{
			for(const auto& command_str_view : commands)
			{
				psl::string command_str{command_str_view};

				bool skip_next_token	 = false;
				psl::char_t opened_scope = '0';
				size_t last_start		 = command_str.find_first_of("'\"");
				while(last_start < command_str.size())
				{
					if((last_start > 0 && command_str[last_start - 1] != '\\') || last_start == 0)
					{
						size_t end = command_str.find_first_of(command_str[last_start], last_start + 1);
						do
						{
							if(command_str[end - 1] == '\\')
							{
								end = command_str.find_first_of(command_str[last_start], end + 1);
								continue;
							}
							end += 2;
							command_str.erase(last_start, end - last_start);
							break;
						} while(end < command_str.size());
					}
					last_start = command_str.find_first_of("'\"", last_start + 1);
				}

				auto tokens{utility::string::split(command_str, " ")};
				tokens.erase(std::remove_if(std::begin(tokens), std::end(tokens),
											[](const auto& view) { return view.size() == 0 || view[0] != '-'; }),
							 std::end(tokens));

				parse(command_str_view, tokens);
			}
		}

		void callback(std::function<void(pack&)> fn) { m_Callback = fn; }

		void operator()()
		{
			if(m_Callback) std::invoke(m_Callback.value(), *this);
		}

	  private:
		void parse(psl::string_view command, const psl::array<psl::string_view>& tokens)
		{
			for(const auto& token : tokens) psl::cout << '\t' << psl::to_platform_string(token) << "\n";
		}
		void validate(const details::value_base& new_value) const
		{
			if(std::any_of(std::begin(m_Values), std::end(m_Values),
						   [&new_value](const auto& value) { return new_value.conflicts(*value.get()); }))
				throw std::runtime_error("conflicting commands found");

			if(std::any_of(std::begin(m_Values), std::end(m_Values),
						   [&new_value](const auto& value) { return new_value.name() == value->name(); }))
				throw std::runtime_error("conflicting name found");
		}

		template <typename T>
		void add(std::shared_ptr<value<T>>& val)
		{
			validate(*val);
			m_Values.emplace_back(val);
		}


		template <typename T>
		void add(value<T>&& val)
		{
			validate(val);
			m_Values.emplace_back(std::make_shared<value<T>>(std::move(val)));
		}

		template <typename T>
		void add(const value<T>& val)
		{
			validate(val);
			m_Values.emplace_back(std::make_shared<value<T>>(val));
		}
		psl::array<std::shared_ptr<details::value_base>> m_Values;
		std::optional<std::function<void(pack&)>> m_Callback;
	};
} // namespace psl::cli