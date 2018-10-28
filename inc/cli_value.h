#pragma once
#include "ustring.h"
#include <optional>
namespace cli
{
	template<typename T>
	class value;

	class value_base
	{
	public:
		value_base() = default;
		value_base(const psl::string& name, const psl::string& cmd, std::optional<psl::char_t> short_cmd, std::optional<psl::string> hint, bool required = false);
		virtual ~value_base() = default;
		value_base(const value_base&) = delete;
		value_base(value_base&& other) noexcept = default;
		value_base& operator=(const value_base&) = delete;
		value_base& operator=(value_base&& other) noexcept = default;
		virtual psl::string help(uint32_t indent = 0) const;

		virtual void reset() = 0;
		virtual bool set(psl::string_view value) = 0;
		virtual bool is_parameter_pack() const { return false; };

		bool required() const;
		psl::string_view name() const;
		psl::string_view command() const;
		std::optional<psl::char_t> short_command() const;
		std::optional<psl::string_view> hint() const;

		template<typename T>
		cli::value<T>* as_a()
		{
			if(ID() == cli::value<T>::m_ID)
				return (cli::value<T>*)(this);
			return nullptr;
		}

	protected:
		value_base& required(bool value);
		value_base& name(psl::string_view value);
		value_base& command(psl::string_view value);
		value_base& short_command(psl::char_t value);
		value_base& hint(psl::string_view value);

		virtual uint64_t ID() const = 0;
	private:
		psl::string m_Name;
		psl::string m_Cmd;
		std::optional<psl::char_t> m_ShortCmd;
		std::optional<psl::string> m_Hint;
		bool m_Required{false};
	protected:
		static uint64_t ID_GENERATOR;
	};

	template<typename T>
	class value final : public value_base
	{
		friend class value_base;
	public:
		value() = default;
		~value()
		{
			if(m_OwnsValue)
				delete(m_Value);
			if(m_OwnsDefault && m_Default)
				delete(m_Default.value());
		}
		value(const value&) = delete;
		value(value&& other) noexcept	: value_base(std::move(other)), m_Value(other.m_Value), m_Default(other.m_Default), m_OwnsValue(other.m_OwnsValue), m_OwnsDefault(other.m_OwnsDefault)
		{
			other.m_OwnsValue = false;
			other.m_OwnsDefault = false;
		};
		value& operator=(const value&) = delete;
		value& operator=(value&& other)
		{
			if(this != &other)
			{
				value_base::operator=(std::move(other));
				m_Value = other.m_Value;
				m_Default = other.m_Default;
				m_OwnsValue = other.m_OwnsValue;
				m_OwnsDefault = other.m_OwnsDefault;
				other.m_OwnsValue = false;
				other.m_OwnsDefault = false;
			}
			return *this;
		};
		bool set(psl::string_view value) override
		{
			if(value.empty())
				return false;
			if(!m_Value)
			{
				m_OwnsValue = true;
				m_Value = new T();
			}
			*m_Value = utility::from_string<T>(psl::to_string8_t(value));
			return true;
		}

		void reset() override
		{
			if(m_Default)
			{
				if(!m_Value)
				{
					m_OwnsValue = true;
					m_Value = new T(*m_Default.value());
				}
				else
				{
					*m_Value = *m_Default.value();
				}
			}
		}
		T* operator->()
		{
			return m_Value;
		}
		T& get()
		{
			if(!m_Value)
			{
				reset();
			}
			if(!m_Value)
			{
				m_OwnsValue = true;
				m_Value = new T();
			}
			return *m_Value;
		}

		value<T>& bind(T& value)
		{
			m_Value = &value;
			return *this;
		}

		value<T>& default_value(const T& value)
		{
			if(m_Default && m_OwnsDefault)
				delete(m_Default.value());

			m_Default = new T(value);
			m_OwnsDefault = true;
			return *this;
		}

		value<T>& bind_default_value(T& value)
		{
			if(m_Default && m_OwnsDefault)
				delete(m_Default.value());

			m_Default = &value;
			m_OwnsDefault = false;
			return *this;
		}

		value<T>& required(bool value)
		{
			value_base::required(value);
			return *this;
		}
		value<T>& name(psl::string_view value)
		{
			value_base::name(value);
			return *this;
		}
		value<T>& command(psl::string_view value)
		{
			value_base::command(value);
			return *this;
		}
		value<T>& short_command(psl::char_t value)
		{
			value_base::short_command(value);
			return *this;
		}
		value<T>& hint(psl::string_view value)
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
		T* m_Value{nullptr};
		std::optional<T*> m_Default{std::nullopt};
		bool m_OwnsDefault = false;
		bool m_OwnsValue = false;

		static uint64_t m_ID;
	};

	template<typename X>
	uint64_t value<X>::m_ID{value_base::ID_GENERATOR++};

	template<>
	class value<bool> final : public value_base
	{
		friend class value_base;
	public:
		value() = default;
		~value()
		{
			if(m_OwnsValue)
				delete(m_Value);
			if(m_Default && m_OwnsDefault)
				delete(m_Default.value());
		}
		value(const value&) = delete;
		value(value&& other) noexcept : value_base(std::move(other)), m_Value(other.m_Value), m_Default(other.m_Default), m_OwnsValue(other.m_OwnsValue), m_OwnsDefault(other.m_OwnsDefault)
		{
			other.m_OwnsValue = false;
			other.m_OwnsDefault = false;
		};
		value& operator=(const value&) = delete;
		value& operator=(value&& other)
		{
			if(this != &other)
			{
				value_base::operator=(std::move(other));
				m_Value = other.m_Value;
				m_Default = other.m_Default;
				m_OwnsValue = other.m_OwnsValue;
				m_OwnsDefault = other.m_OwnsDefault;
				other.m_OwnsValue = false;
				other.m_OwnsDefault = false;
			}
			return *this;
		};
		bool set(psl::string_view value) override
		{
			if(!m_Value)
			{
				m_OwnsValue = true;
				if(m_Default)
					m_Value = new bool(!*m_Default.value());
				else
					m_Value = new bool(true);
			}
			else
			{
				if(m_Default)
					*m_Value = !*m_Default.value();
				else
					*m_Value = !*m_Value;
			}
			if(!value.empty())
				*m_Value = utility::from_string<bool>(psl::to_string8_t(value));
			return true;
		}

		void reset() override
		{
			if(!m_Value)
			{
				m_OwnsValue = true;
				m_Value = new bool();
			}

			if(m_Default)
			{
				*m_Value = *m_Default.value();
			}
			else
			{
				*m_Value = false;
			}
		};
		

		bool* operator->()
		{
			return m_Value;
		}
		bool& get()
		{
			return *m_Value;
		}

		value<bool>& bind(bool& value)
		{
			m_Value = &value;
			return *this;
		}

		value<bool>& default_value(const bool& value)
		{
			if(m_Default && m_OwnsDefault)
				delete(m_Default.value());

			m_Default = new bool(value);
			m_OwnsDefault = true;
			return *this;
		}

		value<bool>& bind_default_value(bool& value)
		{

			if(m_Default && m_OwnsDefault)
				delete(m_Default.value());

			m_Default = &value;
			m_OwnsDefault = false;
			return *this;
		}

		value<bool>& required(bool value)
		{
			value_base::required(value);
			return *this;
		}
		value<bool>& name(psl::string_view value)
		{
			value_base::name(value);
			return *this;
		}
		value<bool>& command(psl::string_view value)
		{
			value_base::command(value);
			return *this;
		}
		value<bool>& short_command(psl::char_t value)
		{
			value_base::short_command(value);
			return *this;
		}
		value<bool>& hint(psl::string_view value)
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
		bool* m_Value{nullptr};
		std::optional<bool*> m_Default;
		bool m_OwnsValue = false;
		bool m_OwnsDefault = false;
		static uint64_t m_ID;
	};
}
