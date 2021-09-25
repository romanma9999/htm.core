/* ---------------------------------------------------------------------
 * HTM Community Edition of NuPIC
 * Copyright (C) 2013, Numenta, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Affero Public License for more details.
 *
 * You should have received a copy of the GNU Affero Public License
 * along with this program.  If not, see http://www.gnu.org/licenses.
 * --------------------------------------------------------------------- */

/** @file
 * Interface for the internal Output class.
 */

#ifndef NTA_OUTPUT_HPP
#define NTA_OUTPUT_HPP

#include <htm/ntypes/Array.hpp>
#include <htm/types/Types.hpp>
#include <htm/utils/Log.hpp> // temporary, while impl is in this file
#include <set>
namespace htm {

class Link;
class Region;
class Array;

/**
 * Represents a named output to a Region.
 */
class Output {
public:
  /**
   * Constructor.
   *
   * @param region
   *        The region that the output belongs to.
   * @param outputName
   *        The region's output name
   * @param type
   *        The type of the output
   */
  Output(Region* region,
         const std::string& outputName, NTA_BasicType type);


  /**
   * Set the name for the output.
   *
   * Output need to know their own name for error messages.
   *
   * @param name
   *        The name of the output
   */
  void setName(const std::string &name);

  /**
   * Get the name of the output.
   *
   * @return
   *        The name of the output
   */
  const std::string &getName() const;

  /**
   * Initialize the Output .
   *
   * @note It's safe to reinitialize an initialized Output with the same
   * parameters.
   *
   */
  void initialize();

  /**
   *
   * Add a Link to the Output .
   *
   * @note The Output does NOT take ownership of @a link, it's created and
   * owned by an Input Object.
   *
   * Called by Input.addLink()
   *
   * @param link
   *        The Link to add
   */
  void addLink(const std::shared_ptr<Link> link);

  /**
   * Removing an existing link from the output.
   *
   * @note Called only by Input.removeLink() even if triggered by
   * Network.removeRegion() while removing the region that contains us.
   *
   * @param link
   *        The Link to remove
   */
  void removeLink(const std::shared_ptr<Link>& link);

  /**
   * Tells whether the output has outgoing links.
   *
   * @note We cannot delete a region if there are any outgoing links
   * This allows us to check in Network.removeRegion() and Network.~Network().
   * @returns
   *         Whether the output has outgoing links
   */
  bool hasOutgoingLinks();

  const std::set<std::shared_ptr<Link>> getLinks() const { return links_; };

  /**
   * Distribute the output to the connected inputs.
   */
  void push();

  /**
   * Get the data of the output.
   * @returns
   *     A reference to the data of the output as an @c Array
   * @note we should return a const Array so caller can't
   * reallocate the buffer. Howerver, we do need to be able to
   * change the content of the buffer. So it cannot be const.
   */
  Array &getData() { return data_; }
  const Array &getData() const { return data_;}

  /**
   *  Get the data type of the output
   */
  NTA_BasicType getDataType() const;


  /**
   *
   * Get the Region that the output belongs to.
   *
   * @returns
   *         The mutable reference to the Region that the output belongs to
   */
  Region* getRegion() const;

  /**
   * Get the count of node output element.
   *
   * @returns
   *         The count of node output element, previously set by initialize().
   */
  size_t getNodeOutputElementCount() const;

  /**
   * Figure out what the dimensions should be for this output buffer.
   * Call this to find out the configured dimensions. Adjust number
   * of dimensions by adding 1's as needed.
   * Then call setDimensions();
   * Call initialize to actually create the buffers. Once buffers are
   * created the dimensions cannot be changed.
   */
  Dimensions determineDimensions();

  /**
   * Get dimensions for this output
   */
  Dimensions &getDimensions() { return dim_; }

  /**
   * Set dimensions for this output
   */
  void setDimensions(const Dimensions& dim) { dim_ = dim; }

  /**
   *  Resize the buffer.  (does not work for SDR or Str buffers)
   *  This is used when a Region needs to change the size of an output buffer at runtime.  (See ClassifierRegion.pdf)
   */
  void resize(size_t size);

  /**
   *  Print raw data...for debugging
   */
  friend std::ostream &operator<<(std::ostream &f, const Output &d);

private:
  // Cannot use the shared_ptr here
  Region* region_;
  Dimensions dim_;
  Array data_;
  // order of links never matters, so store as a set
  // this is different from Input, where they do matter
  std::set<std::shared_ptr<Link>> links_;
  std::string name_;
};


} // namespace htm

#endif // NTA_OUTPUT_HPP
