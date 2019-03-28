#pragma once
#include <memory>
#include <optional>
#include "vector.h"
#include "array.h"
#include "ustring.h"

namespace psl::cli
{
	namespace details
	{
		static psl::static_array<psl::string8::char_t, 4> illegal_characters{' ', '"', '\'', '-'};
		class value_base
		{
		  public:
			value_base(psl::string8_t name, const psl::array<psl::string8_t>& commands) : m_Name(name)
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
			virtual bool optional() const noexcept = 0;
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
		};
	} // namespace details

	template <typename T, bool Optional = true>
	class value final : public details::value_base
	{
	  public:
		value(psl::string8_t name, const psl::array<psl::string8_t>& commands,
			  std::optional<std::shared_ptr<T>> default_value = std::nullopt)
			: details::value_base(name, commands), m_Default(default_value){};
		value(psl::string8_t name, const psl::array<psl::string8_t>& commands, T default_value)
			: details::value_base(name, commands), m_Default(std::make_shared<T>(default_value)){};
		bool optional() const noexcept override { return Optional; }

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
				*m_Value = val;
			}
		}

		void clear() { m_Value = std::nullopt; }

		void set_default(const T& val) { m_Default = std::make_shared<T>(val); }
		void set_default(std::shared_ptr<T>& val) { m_Default = val; }

	  private:
		std::optional<std::shared_ptr<T>> m_Value;
		std::optional<std::shared_ptr<T>> m_Default;
	};

	class pack final
	{
	  public:
		template <typename... Ts>
		pack(Ts&&... values)
		{
			(add(std::forward<Ts>(values)), ...);
		}

		auto begin() noexcept { return std::begin(m_Values); }
		auto end() noexcept { return std::end(m_Values); }
		auto cbegin() const noexcept { return std::cbegin(m_Values); }
		auto cend() const noexcept { return std::cend(m_Values); }

	  private:
		template <typename T>
		void add(std::shared_ptr<value<T>>& val)
		{
			m_Values.emplace_back(val);
		}


		template <typename T>
		void add(value<T>&& val)
		{
			m_Values.emplace_back(std::make_shared<value<T>>(std::move(val)));
		}

		template <typename T>
		void add(const value<T>& val)
		{
			m_Values.emplace_back(std::make_shared<value<T>>(val));
		}
		psl::array<std::shared_ptr<details::value_base>> m_Values;
	};
} // namespace psl::cli