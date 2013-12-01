using System;
using System.Runtime.Serialization;

namespace uk.org.greenend.DisOrder
{
  /// <summary>
  /// Exception representing an error parsing a string
  /// </summary>
  /// <remarks><para>Used in configuration file and protocol parsing.</para></remarks>
  [Serializable()]
  public class InvalidStringException: Exception {
    public InvalidStringException() : base() { }
    public InvalidStringException(string message) : base(message) { }
    public InvalidStringException(string message, System.Exception inner) : base(message, inner) { }
    protected InvalidStringException(SerializationInfo info, StreamingContext context) { }
  }

  /// <summary>
  /// Exception representing an error parsing the configuration file
  /// </summary>
  [Serializable()]
  public class InvalidConfigurationException : Exception
  {
    public InvalidConfigurationException() : base() { }
    public InvalidConfigurationException(string message) : base(message) { }
    public InvalidConfigurationException(string message, System.Exception inner) : base(message, inner) { }
    protected InvalidConfigurationException(SerializationInfo info, StreamingContext context) { }

    public string Path = null;
    public int Line = 0;
  }

  /// <summary>
  /// Exception representing an error parsing a server response
  /// </summary>
  [Serializable()]
  public class InvalidServerResponseException : Exception
  {
    public InvalidServerResponseException() : base() { }
    public InvalidServerResponseException(string message) : base(message) { }
    public InvalidServerResponseException(string message, System.Exception inner) : base(message, inner) { }
    protected InvalidServerResponseException(SerializationInfo info, StreamingContext context) { }
  }
}