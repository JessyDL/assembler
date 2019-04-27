#pragma once
#include <memory>
#include <optional>
#include "vector.h"
#include "array.h"
#include "ustring.h"
#include "string_utils.h"
#include "memory/region.h"
#include <queue>

namespace psl
{
	psl::string8_t::const_iterator find_substring_index(const psl::string8_t& str, psl::string8::view view)
	{
		if(view.data() < str.data() || view.data() >= str.data() + str.size()) return std::end(str);

		return std::next(std::begin(str), view.data() - str.data());
	}

	psl::string8_t::iterator find_substring_index(psl::string8_t& str, psl::string8::view view)
	{
		if(view.data() < str.data() || view.data() >= str.data() + str.size()) return std::end(str);

		return std::next(std::begin(str), view.data() - str.data());
	}

	psl::string16_t::const_iterator find_substring_index(const psl::string16_t& str, psl::string16::view view)
	{
		if(view.data() < str.data() || view.data() >= str.data() + str.size()) return std::end(str);

		return std::next(std::begin(str), view.data() - str.data());
	}

	psl::string16_t::iterator find_substring_index(psl::string16_t& str, psl::string16::view view)
	{
		if(view.data() < str.data() || view.data() >= str.data() + str.size()) return std::end(str);

		return std::next(std::begin(str), view.data() - str.data());
	}

	psl::string32_t::const_iterator find_substring_index(const psl::string32_t& str, psl::string32::view view)
	{
		if(view.data() < str.data() || view.data() >= str.data() + str.size()) return std::end(str);

		return std::next(std::begin(str), view.data() - str.data());
	}

	psl::string32_t::iterator find_substring_index(psl::string32_t& str, psl::string32::view view)
	{
		if(view.data() < str.data() || view.data() >= str.data() + str.size()) return std::end(str);

		return std::next(std::begin(str), view.data() - str.data());
	}

	psl::string8::view::iterator find_substring_index(psl::string8::view str, psl::string8::view view)
	{
		if(view.data() < str.data() || view.data() >= str.data() + str.size()) return std::end(str);

		return std::next(std::begin(str), view.data() - str.data());
	}

	psl::string16::view::iterator find_substring_index(psl::string16::view str, psl::string16::view view)
	{
		if(view.data() < str.data() || view.data() >= str.data() + str.size()) return std::end(str);

		return std::next(std::begin(str), view.data() - str.data());
	}

	psl::string32::view::iterator find_substring_index(psl::string32::view str, psl::string32::view view)
	{
		if(view.data() < str.data() || view.data() >= str.data() + str.size()) return std::end(str);

		return std::next(std::begin(str), view.data() - str.data());
	}
} // namespace psl

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

			template <typename T>
			bool is_a() const noexcept
			{
				return m_ID == key_for<T>();
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
						return;
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

	  private:
		size_t next_instance_of(psl::string_view view, psl::char_t delimiter, size_t offset = 0) const noexcept
		{
			offset = view.find(delimiter, offset);
			while(offset != view.npos)
			{
				if(offset == 0 || (offset > 0 && view[offset - 1] != '\\')) return offset;
				offset = view.find(delimiter, offset + 1);
			}
			return offset;
		}

		bool occupied(size_t index, const psl::array<std::pair<size_t, size_t>>& range) const noexcept
		{
			return std::any_of(std::begin(range), std::end(range), [index](const std::pair<size_t, size_t> r) {
				return r.first <= index && index <= r.second;
			});
		}

		struct command
		{
			psl::string_view cmd;
			psl::string_view args;
			bool is_long_command{false};
		};

	  public:
		void parse(psl::array<psl::string_view> commands)
		{
			for(const auto& view : commands)
			{
				// todo: support the ' delimiter
				psl::array<std::pair<size_t, size_t>> occupied_ranges{};

				for(size_t token_offset = next_instance_of(view, '"', 0); token_offset != psl::string_view::npos;
					token_offset		= next_instance_of(view, '"', token_offset + 1))
				{
					auto begin_token = token_offset + 1;
					token_offset	 = next_instance_of(view, '"', begin_token);
					if(token_offset == psl::string_view::npos)
					{
						// error out
						psl::cerr << "ERROR: opened a delimiter (\"), but did not close it again.\n";
						return;
					}
					occupied_ranges.emplace_back(std::pair{begin_token, token_offset});
				}

				psl::array<command> commands{};
				for(size_t token_offset = view.find('-'); token_offset != psl::string_view::npos;
					token_offset		= view.find('-', token_offset + 1))
				{
					if(occupied(token_offset, occupied_ranges)) continue;
					bool is_long_command = (token_offset != view.size() - 1) && view[token_offset + 1] == '-';
					token_offset += (is_long_command) ? 2 : 1;

					size_t cmd_start = token_offset;
					size_t cmd_end   = view.find(' ', cmd_start);
					size_t arg_start = view.find_first_not_of(' ', cmd_end);
					size_t arg_end   = view.find('-', token_offset);

					cmd_end   = (cmd_end == view.npos) ? view.size() : cmd_end;
					arg_start = (arg_start == view.npos) ? view.size() : arg_start;

					while(arg_end != psl::string_view::npos && occupied(arg_end, occupied_ranges))
					{
						arg_end = view.find('-', arg_end + 1);
					}
					if(arg_end == psl::string_view::npos) arg_end = view.size();

					if(!is_long_command && cmd_end - cmd_start > 1)
					{
						psl::cerr << "ERROR: a short command cannot have multiple characters, please check: -"
								  << psl::to_platform_string(
										 psl::string_view(view.data() + cmd_start, cmd_end - cmd_start))
								  << "\n";
						return;
					}
					if(!is_long_command && cmd_end - cmd_start == 0)
					{
						psl::cerr << "ERROR: a short command supplied, but no character behind it, please check the "
									 "free floating '-' \n";
						return;
					}
					if(cmd_end != cmd_start)
					{
						commands.emplace_back(command{psl::string_view(view.data() + cmd_start, cmd_end - cmd_start),
													  psl::string_view(view.data() + arg_start, arg_end - arg_start),
													  is_long_command});
					}
				}
				std::queue<pack*> processed_packs;
				if(auto end = parse(std::begin(commands), std::end(commands), processed_packs);
				   end != std::end(commands))
				{
					psl::cout << "ERROR: invalid command detected. the command '" << psl::to_platform_string(end->cmd)
							  << "' was not found.\n";
					return;
				}

				while(processed_packs.size() > 0)
				{
					processed_packs.front()->operator()();
					processed_packs.pop();
				}
			}
		}

		void callback(std::function<void(pack&)> fn) { m_Callback = fn; }

		void operator()()
		{
			if(m_Callback) std::invoke(m_Callback.value(), *this);
		}

	  private:
		bool has_command(psl::string_view cmd)
		{
			return std::any_of(
				std::begin(m_Values), std::end(m_Values),
				[&cmd](const std::shared_ptr<details::value_base>& v) { return v->contains_command(cmd); });
		}

		bool is_pack(psl::string_view cmd)
		{
			return std::any_of(std::begin(m_Values), std::end(m_Values),
							   [&cmd](const std::shared_ptr<details::value_base>& v) {
								   return v->contains_command(cmd) && v->is_a<cli::pack>();
							   });
		}

		std::shared_ptr<details::value_base> get_value(const command& cmd)
		{
			auto index = std::find_if(
				std::begin(m_Values), std::end(m_Values),
				[&cmd](const std::shared_ptr<details::value_base>& v) { return v->contains_command(cmd.cmd); });
			if(index == std::end(m_Values)) return nullptr;
			return *index;
		}

		psl::array<command>::const_iterator parse(psl::array<command>::const_iterator begin,
												  psl::array<command>::const_iterator end,
												  std::queue<pack*>& processed_packs)
		{
			processed_packs.push(this);

			for(auto command_it = begin; command_it != end; command_it = std::next(command_it))
			{
				auto value = get_value(*command_it);
				while(!value)
				{
					return command_it;
				}
				if(value->is_a<cli::pack>())
				{
					pack* new_child = value->as<cli::pack>().get_shared().operator->();
					if(new_child != this)
					{
						command_it = std::prev(new_child->parse(std::next(command_it), end, processed_packs));
					}
				}
				else
				{
					value->from_string(command_it->args);
				}
				// psl::cout << '\t' << "command: " << psl::to_platform_string(command_it->cmd)
				//		  << " depth: " << psl::to_platform_string(std::to_string(stack.size() - 1))
				//		  << "\n\t\t argument: " << psl::to_platform_string(command_it->args) << "\n";
			}
			return end;
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