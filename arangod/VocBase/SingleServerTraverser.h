////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_SINGLE_SERVER_TRAVERSER_H
#define ARANGOD_SINGLE_SERVER_TRAVERSER_H 1

#include "VocBase/Traverser.h"
#include "Aql/AqlValue.h"
#include "Utils/OperationCursor.h"
#include "VocBase/PathEnumerator.h"

namespace arangodb {

class EdgeIndex;

namespace traverser {

class PathEnumerator;

class SingleServerTraverser final : public Traverser {

 private:

  class VertexGetter {
   public:
    explicit VertexGetter(SingleServerTraverser* traverser)
        : _traverser(traverser) {}

    virtual ~VertexGetter() = default;

    virtual bool getVertex(std::string const&, std::string const&, size_t,
                           std::string&);
    virtual void reset(std::string const&);

   protected:
    SingleServerTraverser* _traverser;
  };

  class UniqueVertexGetter : public VertexGetter {
   public:
    explicit UniqueVertexGetter(SingleServerTraverser* traverser)
        : VertexGetter(traverser) {}

    ~UniqueVertexGetter() = default;

    bool getVertex(std::string const&, std::string const&, size_t,
                    std::string&) override;

    void reset(std::string const&) override;

   private:
    std::unordered_set<std::string> _returnedVertices;
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief callable class to load edges based on opts.
  //////////////////////////////////////////////////////////////////////////////

  class EdgeGetter {

   public:
    EdgeGetter(SingleServerTraverser* traverser,
               TraverserOptions const& opts,
               Transaction* trx)
        : _traverser(traverser), _opts(opts), _trx(trx) {}

    //////////////////////////////////////////////////////////////////////////////
    /// @brief Function to fill the list of edges properly.
    //////////////////////////////////////////////////////////////////////////////

    void getEdge(std::string const&, std::vector<std::string>&,
                 size_t*&, size_t&);

    void getAllEdges(std::string const&, std::unordered_set<std::string>&, size_t);

   private:

    //////////////////////////////////////////////////////////////////////////////
    /// @brief Get the next valid cursor
    //////////////////////////////////////////////////////////////////////////////

    bool nextCursor(std::string const&, size_t&, size_t*&);

    //////////////////////////////////////////////////////////////////////////////
    /// @brief Get the next edge
    //////////////////////////////////////////////////////////////////////////////

    void nextEdge(std::string const&, size_t&, size_t*&,
                  std::vector<std::string>&);

    SingleServerTraverser* _traverser;

    //////////////////////////////////////////////////////////////////////////////
    /// @brief Traverser options
    //////////////////////////////////////////////////////////////////////////////

    TraverserOptions _opts;

    //////////////////////////////////////////////////////////////////////////////
    /// @brief Pointer to active transaction
    ///        All Edge Collections have to be properly locked before traversing!
    //////////////////////////////////////////////////////////////////////////////

    Transaction* _trx;

    //////////////////////////////////////////////////////////////////////////////
    /// @brief Stack of all active cursors
    //////////////////////////////////////////////////////////////////////////////

    std::stack<std::unique_ptr<OperationCursor>> _cursors;

    //////////////////////////////////////////////////////////////////////////////
    /// @brief Stack of all active cursor batches
    //////////////////////////////////////////////////////////////////////////////

    std::stack<std::vector<TRI_doc_mptr_t*>> _results;

    //////////////////////////////////////////////////////////////////////////////
    /// @brief Stack of positions in the cursors
    //////////////////////////////////////////////////////////////////////////////

    std::stack<size_t> _posInCursor;

    //////////////////////////////////////////////////////////////////////////////
    /// @brief velocyPack builder to create temporary search values
    //////////////////////////////////////////////////////////////////////////////

    arangodb::velocypack::Builder _builder;
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief internal cursor to enumerate the paths of a graph
  //////////////////////////////////////////////////////////////////////////////

  std::unique_ptr<arangodb::traverser::PathEnumerator> _enumerator;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief internal getter to extract an edge
  //////////////////////////////////////////////////////////////////////////////

  std::unique_ptr<EdgeGetter> _edgeGetter;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief internal getter to extract an edge
  //////////////////////////////////////////////////////////////////////////////

  std::unique_ptr<VertexGetter> _vertexGetter;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief a vector containing all required edge collection structures
  //////////////////////////////////////////////////////////////////////////////

public:
 SingleServerTraverser(TraverserOptions&, Transaction*);

 ~SingleServerTraverser();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Reset the traverser to use another start vertex
  //////////////////////////////////////////////////////////////////////////////

  void setStartVertex(std::string const& v) override;


  //////////////////////////////////////////////////////////////////////////////
  /// @brief Function to load edges for a node
  //////////////////////////////////////////////////////////////////////////////

  void getEdge(std::string const&, std::vector<std::string>&, size_t*&,
               size_t&) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Function to load all edges for a list of nodes
  //////////////////////////////////////////////////////////////////////////////

  void getAllEdges(std::string const&, std::unordered_set<std::string>&,
                   size_t) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Function to load the other sides vertex of an edge
  ///        Returns true if the vertex passes filtering conditions
  //////////////////////////////////////////////////////////////////////////////

  bool getVertex(std::string const&, std::string const&, size_t,
                 std::string&) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Get the next possible path in the graph.
  //////////////////////////////////////////////////////////////////////////////

  bool next() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Builds only the last vertex as AQLValue
  //////////////////////////////////////////////////////////////////////////////

  aql::AqlValue lastVertexToAqlValue() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Builds only the last edge as AQLValue
  //////////////////////////////////////////////////////////////////////////////

  aql::AqlValue lastEdgeToAqlValue() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Builds the complete path as AQLValue
  ///        Has the format:
  ///        {
  ///           vertices: [<vertex-as-velocypack>],
  ///           edges: [<edge-as-velocypack>]
  ///        }
  ///        NOTE: Will clear the given buffer and will leave the path in it.
  //////////////////////////////////////////////////////////////////////////////

  aql::AqlValue pathToAqlValue(arangodb::velocypack::Builder&) override;

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Function to fetch the real data of a vertex into an AQLValue
  //////////////////////////////////////////////////////////////////////////////

  aql::AqlValue fetchVertexData(std::string const&) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Function to fetch the real data of an edge into an AQLValue
  //////////////////////////////////////////////////////////////////////////////

  aql::AqlValue fetchEdgeData(std::string const&) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Function to add the real data of a vertex into a velocypack builder
  //////////////////////////////////////////////////////////////////////////////

  void addVertexToVelocyPack(std::string const&,
                             arangodb::velocypack::Builder&) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Function to add the real data of an edge into a velocypack builder
  //////////////////////////////////////////////////////////////////////////////

  void addEdgeToVelocyPack(std::string const&,
                           arangodb::velocypack::Builder&) override;

 private:
  bool edgeMatchesConditions(arangodb::velocypack::Slice, size_t);

  bool vertexMatchesConditions(std::string const&, size_t);

  std::vector<TRI_document_collection_t*> _edgeCols;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Outer top level transaction
  //////////////////////////////////////////////////////////////////////////////

  Transaction* _trx;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Cache for vertex documents, points from _id to start of 
  /// document VPack value (in datafiles)
  //////////////////////////////////////////////////////////////////////////////

  std::unordered_map<std::string, uint8_t const*> _vertices;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Cache for edge documents, points from _id to start of edge
  /// VPack value (in datafiles)
  //////////////////////////////////////////////////////////////////////////////

  std::unordered_map<std::string, uint8_t const*> _edges;

};
} // namespace traverser
} // namespace arangodb

#endif
