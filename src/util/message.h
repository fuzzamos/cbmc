/*******************************************************************\

Module:

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/


#ifndef CPROVER_UTIL_MESSAGE_H
#define CPROVER_UTIL_MESSAGE_H

#include <string>
#include <iosfwd>
#include <sstream>

#include "invariant.h"
#include "json.h"
#include "source_location.h"
#include "xml.h"

class json_stream_arrayt;

class message_handlert
{
public:
  message_handlert():verbosity(10), message_count(10, 0)
  {
  }

  virtual void print(unsigned level, const std::string &message)=0;

  virtual void print(unsigned level, const xmlt &xml)
  {
    // no-op by default
  }

  /// Return the underlying JSON stream
  virtual json_stream_arrayt &get_json_stream()
  {
    UNREACHABLE;
  }

  virtual void print(unsigned level, const jsont &json)
  {
    // no-op by default
  }

  virtual void print(
    unsigned level,
    const std::string &message,
    int sequence_number,
    const source_locationt &location);

  virtual void flush(unsigned level)
  {
    // no-op by default
  }

  virtual ~message_handlert()
  {
  }

  void set_verbosity(unsigned _verbosity) { verbosity=_verbosity; }
  unsigned get_verbosity() const { return verbosity; }

  unsigned get_message_count(unsigned level) const
  {
    if(level>=message_count.size())
      return 0;

    return message_count[level];
  }

protected:
  unsigned verbosity;
  std::vector<unsigned> message_count;
};

class null_message_handlert:public message_handlert
{
public:
  virtual void print(unsigned level, const std::string &message)
  {
    message_handlert::print(level, message);
  }

  virtual void print(
    unsigned level,
    const std::string &message,
    int sequence_number,
    const source_locationt &location)
  {
    print(level, message);
  }
};

class stream_message_handlert:public message_handlert
{
public:
  explicit stream_message_handlert(std::ostream &_out):out(_out)
  {
  }

  virtual void print(unsigned level, const std::string &message)
  {
    message_handlert::print(level, message);

    if(verbosity>=level)
      out << message << '\n';
  }

  virtual void flush(unsigned level)
  {
    out << std::flush;
  }

protected:
  std::ostream &out;
};

class messaget
{
public:
  // Standard message levels:
  //
  //  0 none
  //  1 only errors
  //  2 + warnings
  //  4 + results
  //  6 + status/phase information
  //  8 + statistical information
  //  9 + progress information
  // 10 + debug info

  enum message_levelt
  {
    M_ERROR=1, M_WARNING=2, M_RESULT=4, M_STATUS=6,
    M_STATISTICS=8, M_PROGRESS=9, M_DEBUG=10
  };

  virtual void set_message_handler(message_handlert &_message_handler)
  {
    message_handler=&_message_handler;
  }

  message_handlert &get_message_handler()
  {
    INVARIANT(
      message_handler!=nullptr,
      "message handler should be set before calling get_message_handler");
    return *message_handler;
  }

  // constructors, destructor

  messaget():
    message_handler(nullptr),
    mstream(M_DEBUG, *this)
  {
  }

  messaget(const messaget &other):
    message_handler(other.message_handler),
    mstream(other.mstream, *this)
  {
  }

  messaget &operator=(const messaget &other)
  {
    message_handler=other.message_handler;
    mstream.assign_from(other.mstream);
    return *this;
  }

  explicit messaget(message_handlert &_message_handler):
    message_handler(&_message_handler),
    mstream(M_DEBUG, *this)
  {
  }

  virtual ~messaget();

  class mstreamt:public std::ostringstream
  {
  public:
    mstreamt(
      unsigned _message_level,
      messaget &_message):
      message_level(_message_level),
      message(_message)
    {
    }

    mstreamt(const mstreamt &other)=delete;

    mstreamt(const mstreamt &other, messaget &_message):
      message_level(other.message_level),
      message(_message),
      source_location(other.source_location)
    {
    }

    mstreamt &operator=(const mstreamt &other)=delete;

    unsigned message_level;
    messaget &message;
    source_locationt source_location;

    mstreamt &operator << (const xmlt &data)
    {
      *this << eom; // force end of previous message
      if(message.message_handler)
      {
        message.message_handler->print(message_level, data);
      }
      return *this;
    }

    mstreamt &operator << (const json_objectt &data)
    {
      *this << eom; // force end of previous message
      if(message.message_handler)
      {
        message.message_handler->print(message_level, data);
      }
      return *this;
    }

    template <class T>
    mstreamt &operator << (const T &x)
    {
      static_cast<std::ostream &>(*this) << x;
      return *this;
    }

    // for feeding in manipulator functions such as eom
    mstreamt &operator << (mstreamt &(*func)(mstreamt &))
    {
      return func(*this);
    }

    /// Returns a reference to the top-level JSON array stream
    json_stream_arrayt &json_stream()
    {
      *this << eom; // force end of previous message
      return message.message_handler->get_json_stream();
    }

  private:
    void assign_from(const mstreamt &other)
    {
      message_level=other.message_level;
      source_location=other.source_location;
      // message, the pointer to my enclosing messaget, remains unaltered.
    }

    friend class messaget;
  };

  // Feeding 'eom' into the stream triggers
  // the printing of the message
  static mstreamt &eom(mstreamt &m)
  {
    if(m.message.message_handler)
    {
      m.message.message_handler->print(
        m.message_level,
        m.str(),
        -1,
        m.source_location);
      m.message.message_handler->flush(m.message_level);
    }
    m.clear(); // clears error bits
    m.str(std::string()); // clears the string
    m.source_location.clear();
    return m;
  }

  // in lieu of std::endl
  static mstreamt &endl(mstreamt &m)
  {
    static_cast<std::ostream &>(m) << std::endl;
    return m;
  }

  mstreamt &get_mstream(unsigned message_level) const
  {
    mstream.message_level=message_level;
    return mstream;
  }

  mstreamt &error() const
  {
    return get_mstream(M_ERROR);
  }

  mstreamt &warning() const
  {
    return get_mstream(M_WARNING);
  }

  mstreamt &result() const
  {
    return get_mstream(M_RESULT);
  }

  mstreamt &status() const
  {
    return get_mstream(M_STATUS);
  }

  mstreamt &statistics() const
  {
    return get_mstream(M_STATISTICS);
  }

  mstreamt &progress() const
  {
    return get_mstream(M_PROGRESS);
  }

  mstreamt &debug() const
  {
    return get_mstream(M_DEBUG);
  }

protected:
  message_handlert *message_handler;
  mutable mstreamt mstream;
};

#endif // CPROVER_UTIL_MESSAGE_H
