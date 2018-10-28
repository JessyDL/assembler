#pragma once
#include "ustring.h"
#include <optional>
#include <vector>
#include "string_utils.h"
#include <iostream>
#include "cli_value.h"
#include <functional>

namespace cli
{
	class parameter_pack
	{
	public:
		struct token
		{
			token(psl::string_view key) : key(key), value{std::nullopt} {};
			token(psl::string_view key, psl::string_view value) : key(key), value(value) {};
			psl::string_view key;
			std::optional<psl::string_view> value;
		};

		// mixed_mode: allows multiple parameters to be set per pack, otherwise only the first one is consumed and others ignored 
		// (unless that first one is a parameter pack itself, in which case it will cascade)
		// fallthrough means that in case an element is a parameter pack, should this pack still continue scanning?
		// when true this can result in weird scenarios if both parameter packs define the same flags/cli::values commands.
		parameter_pack(bool mixed_mode = true, bool fallthrough = false, bool warn_on_unknown_parameter = true)
			: 
			m_Mixed(mixed_mode), 
			m_Fallthrough(fallthrough),
			m_WarnUnknownParam(warn_on_unknown_parameter)
		{

		}
		std::optional<std::reference_wrapper<cli::value_base>> operator[](psl::string_view name)
		{
			validate();
			for(const auto& value : m_FlattenedValues)
			{
				if(value->name() == name)
					return *value;
			}
			return std::nullopt;
		}

		template<typename T>
		cli::value<T>* get_as_a(psl::string_view name)
		{
			validate();
			for(const auto& value : m_FlattenedValues)
			{
				if(value->name() == name)
				{
					return value->as_a<T>();
				}
			}
			throw std::runtime_error("no value in pack with given name");
			return nullptr;
		}

		void add(std::shared_ptr<cli::value_base>& value)
		{
			m_Dirty = true;
			m_Values.emplace_back(value);
		}

		template<typename T>
		parameter_pack& add(cli::value<T>&& value)
		{
			m_Dirty = true;
			m_Values.emplace_back(std::make_shared<cli::value<T>>(std::forward<cli::value<T>>(value)));
			return *this;
		}


		template<typename... Types>
		parameter_pack& add(Types&&... values)
		{
			m_Dirty = true;
			add(std::forward<Types>(values)...);
			return *this;
		}

		template<typename T>
		parameter_pack& operator<<(cli::value<T>&& value)
		{
				m_Dirty = true;
			m_Values.emplace_back(std::make_shared<cli::value<T>>(std::forward<cli::value<T>>(value)));
			return *this;
		}

		bool mixed_mode() const
		{
			return m_Mixed;
		}

		void mixed_mode(bool value)
		{
			m_Mixed = value;
		}

		bool fallthrough() const
		{
			return m_Fallthrough;
		}

		void fallthrough(bool value)
		{
			m_Fallthrough = value;
		}

		bool warn_on_unknown_parameter() const
		{
			return m_WarnUnknownParam;
		}

		void warn_on_unknown_parameter(bool value)
		{
			m_WarnUnknownParam = value;
		}
		
		void clear()
		{
			m_Dirty = true;
			m_Values.clear();
		}

		psl::string help(bool header = true, uint32_t indent = 0)
		{
			if(!validate())
				return "";
			psl::string res =((header)? psl::string(indent * 2, ' ') + "usage info of the parameters:\n":"");

			for(const auto& value : m_FlattenedValues)
			{
				res += value->help(indent +1);
			}

			return res;
		}


		bool parse(const std::vector<token>& tokens, std::vector<cli::parameter_pack*>& parsed_packs);
		bool parse(psl::string_view parameters);

		void callback(std::function<void(parameter_pack&)>&& cb)
		{
			m_Callback = std::move(cb);
		}
		void callback(const std::function<void(parameter_pack&)>& cb)
		{
			m_Callback = cb;
		}

		void unset_callback()
		{
			m_Callback = std::nullopt;
		}

		bool is_valid() const;
		bool validate();

		void reset_values_to_default()
		{
			for(auto& val : m_FlattenedValues)
			{
				val->reset();
			}
		}
	private:
		std::vector<std::shared_ptr<value_base>> m_Values{};
		std::vector<std::function<void(parameter_pack&)>> m_Callbacks;
		std::vector<std::shared_ptr<value_base>> m_FlattenedValues{};
		std::vector<std::shared_ptr<value_base>> m_ParsedValues{};
		bool m_Mixed = true;
		bool m_Fallthrough = false;
		bool m_WarnUnknownParam = true;
		std::optional<std::function<void(parameter_pack&)>> m_Callback;
		bool m_DontInvokeCB = false;
		bool m_Dirty = true;
		bool m_Valid = false;
	};
	template<>
	class value<parameter_pack> : public value_base
	{
		friend class value_base;
	public:
		value(parameter_pack& pack) : m_Value(pack) {};
		~value() {};
		value(const value&) = delete;
		value(value&& other) noexcept : value_base(std::move(other)), m_Value(other.m_Value)
		{
		};
		value& operator=(const value&) = delete;
		value& operator=(value&&) = delete;

		bool set(psl::string_view value) override
		{
			return false;
		}
		virtual bool is_parameter_pack() const { return true; };

		void reset() override
		{
			m_Value.reset_values_to_default();
		};
		psl::string help(uint32_t indent = 0) const override
		{
			psl::string res = value::value_base::help(indent);
			res += m_Value.help(false, indent);
			return res;
		}

		parameter_pack* operator->()
		{
			return &m_Value;
		}
		parameter_pack& get()
		{
			return m_Value;
		}

		value<parameter_pack>& rebind(parameter_pack& value)
		{
			m_Value = value;
			return *this;
		}

		value<parameter_pack>& required(bool value)
		{
			value_base::required(value);
			return *this;
		}
		value<parameter_pack>& name(psl::string_view value)
		{
			value_base::name(value);
			return *this;
		}
		value<parameter_pack>& command(psl::string_view value)
		{
			value_base::command(value);
			return *this;
		}
		value<parameter_pack>& short_command(psl::char_t value)
		{
			value_base::short_command(value);
			return *this;
		}
		value<parameter_pack>& hint(psl::string_view value)
		{
			value_base::hint(value);
			return *this;
		}

	protected:
		uint64_t ID() const override
		{
			return m_ID;
		};

	private:
		parameter_pack& m_Value;
		static uint64_t m_ID;
	};

}
